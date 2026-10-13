// Fill out your copyright notice in the Description page of Project Settings.

#include "Core/TextureToolkitImageUtils.h"

#include "Engine/Texture2D.h"

#include "TextureToolkitLog.h"

#define LOCTEXT_NAMESPACE "TextureToolkitImageUtils"

bool FTextureToolkitImageUtils::CanReadSource(const UTexture2D* InTexture, FText* OutReason)
{
    FText reason;
    bool bResult = false;

    if (!InTexture)
    {
        reason = LOCTEXT("NoTexture", "No texture.");
    }
    else if (!InTexture->Source.IsValid())
    {
        reason = LOCTEXT("NoSource", "Texture has no source data.");
    }
    else if (InTexture->Source.GetNumBlocks() > 1)
    {
        reason = LOCTEXT("Udim", "UDIM textures are not supported.");
    }
    else if (InTexture->Source.GetNumLayers() > 1)
    {
        reason = LOCTEXT("Layers", "Multi layer textures are not supported.");
    }
    else
    {
        bResult = true;
    }

    if (OutReason)
    {
        *OutReason = reason;
    }
    return bResult;
}

bool FTextureToolkitImageUtils::ReadSourceImage(UTexture2D* InTexture, FImage& OutImage)
{
    FText reason;
    if (!CanReadSource(InTexture, &reason))
    {
        UE_LOG(LogTextureToolkit, Warning, TEXT("[FTextureToolkitImageUtils] Cannot read %s: %s"), *GetNameSafe(InTexture), *reason.ToString());
        return false;
    }

    if (!InTexture->Source.GetMipImage(OutImage, 0))
    {
        UE_LOG(LogTextureToolkit, Warning, TEXT("[FTextureToolkitImageUtils] Failed to read source mip of %s"), *InTexture->GetPathName());
        return false;
    }
    return true;
}

bool FTextureToolkitImageUtils::ReadRawImage(UTexture2D* InTexture, FImage& OutImage)
{
    FImage sourceImage;
    if (!ReadSourceImage(InTexture, sourceImage))
    {
        return false;
    }

    if (ERawImageFormat::GetFormatNeedsGammaSpace(sourceImage.Format))
    {
        sourceImage.GammaSpace = EGammaSpace::Linear;
    }
    sourceImage.CopyTo(OutImage, ERawImageFormat::RGBA32F, EGammaSpace::Linear);
    return true;
}

bool FTextureToolkitImageUtils::ReadDisplayImage(UTexture2D* InTexture, FImage& OutImage)
{
    FImage sourceImage;
    if (!ReadSourceImage(InTexture, sourceImage))
    {
        return false;
    }

    sourceImage.CopyTo(OutImage, ERawImageFormat::BGRA8, EGammaSpace::sRGB);
    return true;
}

FIntRect FTextureToolkitImageUtils::ComputeOpaqueBounds(const FImage& InImage, uint8 InAlphaThreshold)
{
    const FIntRect fullRect(0, 0, InImage.SizeX, InImage.SizeY);
    if (InImage.Format != ERawImageFormat::BGRA8 || InImage.SizeX <= 0 || InImage.SizeY <= 0)
    {
        return fullRect;
    }

    const TArrayView64<const FColor> pixels = InImage.AsBGRA8();
    int32 minX = InImage.SizeX;
    int32 minY = InImage.SizeY;
    int32 maxX = -1;
    int32 maxY = -1;

    for (int32 y = 0; y < InImage.SizeY; ++y)
    {
        const int64 rowOffset = static_cast<int64>(y) * InImage.SizeX;
        for (int32 x = 0; x < InImage.SizeX; ++x)
        {
            if (pixels[rowOffset + x].A > InAlphaThreshold)
            {
                minX = FMath::Min(minX, x);
                maxX = FMath::Max(maxX, x);
                minY = FMath::Min(minY, y);
                maxY = FMath::Max(maxY, y);
            }
        }
    }

    if (maxX < 0)
    {
        return fullRect;
    }
    return FIntRect(minX, minY, maxX + 1, maxY + 1);
}

void FTextureToolkitImageUtils::CropImage(const FImage& InSource, const FIntRect& InRect, FImage& OutImage)
{
    FIntRect rect = InRect;
    rect.Clip(FIntRect(0, 0, InSource.SizeX, InSource.SizeY));

    const int32 width = FMath::Max(0, rect.Width());
    const int32 height = FMath::Max(0, rect.Height());
    OutImage.Init(width, height, InSource.Format, InSource.GammaSpace);
    if (width == 0 || height == 0)
    {
        return;
    }

    const int64 rowBytes = static_cast<int64>(width) * InSource.GetBytesPerPixel();
    for (int32 y = 0; y < height; ++y)
    {
        const void* src = InSource.GetPixelPointer(rect.Min.X, rect.Min.Y + y);
        void* dst = OutImage.GetPixelPointer(0, y);
        FMemory::Memcpy(dst, src, rowBytes);
    }
}

void FTextureToolkitImageUtils::BlitImage(const FImage& InSource, FImage& InOutDest, const FIntPoint& InDestPosition)
{
    if (InSource.Format != InOutDest.Format)
    {
        UE_LOG(LogTextureToolkit, Warning, TEXT("[FTextureToolkitImageUtils] BlitImage format mismatch."));
        return;
    }

    FIntRect destRect(InDestPosition, InDestPosition + FIntPoint(InSource.SizeX, InSource.SizeY));
    destRect.Clip(FIntRect(0, 0, InOutDest.SizeX, InOutDest.SizeY));
    if (destRect.Width() <= 0 || destRect.Height() <= 0)
    {
        return;
    }

    const FIntPoint sourceOffset = destRect.Min - InDestPosition;
    const int64 rowBytes = static_cast<int64>(destRect.Width()) * InSource.GetBytesPerPixel();
    for (int32 y = 0; y < destRect.Height(); ++y)
    {
        const void* src = InSource.GetPixelPointer(sourceOffset.X, sourceOffset.Y + y);
        void* dst = InOutDest.GetPixelPointer(destRect.Min.X, destRect.Min.Y + y);
        FMemory::Memcpy(dst, src, rowBytes);
    }
}

void FTextureToolkitImageUtils::ExtrudeEdges(FImage& InOutImage, const FIntRect& InRect, int32 InAmount)
{
    if (InAmount <= 0 || InRect.Width() <= 0 || InRect.Height() <= 0)
    {
        return;
    }

    const int64 bytesPerPixel = InOutImage.GetBytesPerPixel();
    const int32 imageWidth = InOutImage.SizeX;
    const int32 imageHeight = InOutImage.SizeY;

    auto copyPixel = [&InOutImage, bytesPerPixel, imageWidth, imageHeight](int32 InSrcX, int32 InSrcY, int32 InDstX, int32 InDstY)
    {
        if (InDstX < 0 || InDstY < 0 || InDstX >= imageWidth || InDstY >= imageHeight)
        {
            return;
        }
        FMemory::Memcpy(InOutImage.GetPixelPointer(InDstX, InDstY), InOutImage.GetPixelPointer(InSrcX, InSrcY), bytesPerPixel);
    };

    const int32 left = InRect.Min.X;
    const int32 top = InRect.Min.Y;
    const int32 right = InRect.Max.X - 1;
    const int32 bottom = InRect.Max.Y - 1;

    for (int32 y = top; y <= bottom; ++y)
    {
        for (int32 step = 1; step <= InAmount; ++step)
        {
            copyPixel(left, y, left - step, y);
            copyPixel(right, y, right + step, y);
        }
    }

    for (int32 x = left - InAmount; x <= right + InAmount; ++x)
    {
        const int32 sourceX = FMath::Clamp(x, left, right);
        for (int32 step = 1; step <= InAmount; ++step)
        {
            copyPixel(sourceX, top, x, top - step);
            copyPixel(sourceX, bottom, x, bottom + step);
        }
    }
}

float FTextureToolkitImageUtils::GetChannelValue(const FLinearColor& InColor, ETextureToolkitChannel InChannel)
{
    switch (InChannel)
    {
    case ETextureToolkitChannel::Red:
        return InColor.R;
    case ETextureToolkitChannel::Green:
        return InColor.G;
    case ETextureToolkitChannel::Blue:
        return InColor.B;
    case ETextureToolkitChannel::Alpha:
        return InColor.A;
    case ETextureToolkitChannel::Luminance:
        return InColor.R * 0.299f + InColor.G * 0.587f + InColor.B * 0.114f;
    default:
        return 0.0f;
    }
}

int64 FTextureToolkitImageUtils::EstimateMemoryBytes(const FIntPoint& InSize, EPixelFormat InFormat, bool bInWithMips)
{
    if (InFormat == PF_Unknown || InFormat >= PF_MAX || InSize.X <= 0 || InSize.Y <= 0)
    {
        return 0;
    }

    const FPixelFormatInfo& formatInfo = GPixelFormats[InFormat];
    if (formatInfo.BlockSizeX <= 0 || formatInfo.BlockSizeY <= 0)
    {
        return 0;
    }

    int64 totalBytes = 0;
    int32 width = InSize.X;
    int32 height = InSize.Y;
    while (true)
    {
        const int64 blocksX = FMath::DivideAndRoundUp(width, formatInfo.BlockSizeX);
        const int64 blocksY = FMath::DivideAndRoundUp(height, formatInfo.BlockSizeY);
        totalBytes += blocksX * blocksY * formatInfo.BlockBytes;

        if (!bInWithMips || (width == 1 && height == 1))
        {
            break;
        }
        width = FMath::Max(1, width / 2);
        height = FMath::Max(1, height / 2);
    }
    return totalBytes;
}

EPixelFormat FTextureToolkitImageUtils::GuessPixelFormat(TextureCompressionSettings InSettings, bool bInHasAlpha)
{
    switch (InSettings)
    {
    case TC_Normalmap:
        return PF_BC5;
    case TC_Grayscale:
    case TC_Displacementmap:
    case TC_DistanceFieldFont:
        return PF_G8;
    case TC_Alpha:
        return PF_BC4;
    case TC_VectorDisplacementmap:
    case TC_EditorIcon:
        return PF_B8G8R8A8;
    case TC_HDR:
        return PF_FloatRGBA;
    case TC_HDR_F32:
        return PF_A32B32G32R32F;
    case TC_HDR_Compressed:
        return PF_BC6H;
    case TC_BC7:
        return PF_BC7;
    case TC_HalfFloat:
        return PF_R16F;
    case TC_SingleFloat:
        return PF_R32_FLOAT;
    case TC_LQ:
        return PF_R5G6B5_UNORM;
    case TC_Default:
    case TC_Masks:
    default:
        return bInHasAlpha ? PF_DXT5 : PF_DXT1;
    }
}

FString FTextureToolkitImageUtils::FormatBytes(int64 InBytes)
{
    if (InBytes >= 1024 * 1024)
    {
        return FString::Printf(TEXT("%.2f MB"), InBytes / (1024.0 * 1024.0));
    }
    if (InBytes >= 1024)
    {
        return FString::Printf(TEXT("%.1f KB"), InBytes / 1024.0);
    }
    return FString::Printf(TEXT("%lld B"), InBytes);
}

FIntPoint FTextureToolkitImageUtils::HalveToFit(const FIntPoint& InSize, int32 InMaxDimension, int32* OutSteps)
{
    FIntPoint result = InSize;
    int32 steps = 0;
    if (InMaxDimension > 0)
    {
        while (FMath::Max(result.X, result.Y) > InMaxDimension && (result.X > 1 || result.Y > 1))
        {
            result.X = FMath::Max(1, result.X / 2);
            result.Y = FMath::Max(1, result.Y / 2);
            ++steps;
        }
    }

    if (OutSteps)
    {
        *OutSteps = steps;
    }
    return result;
}

FIntPoint FTextureToolkitImageUtils::ScaleToFit(const FIntPoint& InSize, int32 InMaxDimension)
{
    const int32 largestSide = FMath::Max(InSize.X, InSize.Y);
    if (InMaxDimension <= 0 || largestSide <= InMaxDimension)
    {
        return InSize;
    }

    if (FMath::IsPowerOfTwo(InSize.X) && FMath::IsPowerOfTwo(InSize.Y))
    {
        return HalveToFit(InSize, InMaxDimension);
    }

    const double scale = static_cast<double>(InMaxDimension) / largestSide;
    return FIntPoint(
        FMath::Clamp(FMath::RoundToInt(InSize.X * scale), 1, InMaxDimension),
        FMath::Clamp(FMath::RoundToInt(InSize.Y * scale), 1, InMaxDimension));
}

#undef LOCTEXT_NAMESPACE
