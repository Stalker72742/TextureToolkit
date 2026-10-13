// Fill out your copyright notice in the Description page of Project Settings.

#include "Core/TextureChannelPacker.h"

#include "Engine/Texture2D.h"
#include "Misc/ScopedSlowTask.h"

#include "Core/TextureToolkitAssetUtils.h"
#include "Core/TextureToolkitImageUtils.h"

#define LOCTEXT_NAMESPACE "TextureChannelPacker"

namespace TextureChannelPackerPrivate
{
    struct FPresetChannel
    {
        FText Label;
        float ConstantValue = 0.0f;
        bool bInvert = false;
        ETextureChannelRole Role = ETextureChannelRole::None;
    };

    void GetPresetChannels(EChannelPackPreset InPreset, FPresetChannel (&OutChannels)[4])
    {
        const FText ao = LOCTEXT("AO", "Ambient Occlusion");
        const FText roughness = LOCTEXT("Roughness", "Roughness");
        const FText metallic = LOCTEXT("Metallic", "Metallic");
        const FText smoothness = LOCTEXT("Smoothness", "Smoothness (from Roughness)");
        const FText unused = LOCTEXT("Unused", "Unused");

        switch (InPreset)
        {
        case EChannelPackPreset::ORM:
            OutChannels[0] = { ao, 1.0f, false, ETextureChannelRole::AmbientOcclusion };
            OutChannels[1] = { roughness, 0.5f, false, ETextureChannelRole::Roughness };
            OutChannels[2] = { metallic, 0.0f, false, ETextureChannelRole::Metallic };
            OutChannels[3] = { unused, 1.0f, false };
            break;
        case EChannelPackPreset::RMA:
            OutChannels[0] = { roughness, 0.5f, false, ETextureChannelRole::Roughness };
            OutChannels[1] = { metallic, 0.0f, false, ETextureChannelRole::Metallic };
            OutChannels[2] = { ao, 1.0f, false, ETextureChannelRole::AmbientOcclusion };
            OutChannels[3] = { unused, 1.0f, false };
            break;
        case EChannelPackPreset::MRA:
            OutChannels[0] = { metallic, 0.0f, false, ETextureChannelRole::Metallic };
            OutChannels[1] = { roughness, 0.5f, false, ETextureChannelRole::Roughness };
            OutChannels[2] = { ao, 1.0f, false, ETextureChannelRole::AmbientOcclusion };
            OutChannels[3] = { unused, 1.0f, false };
            break;
        case EChannelPackPreset::MaskMap:
            OutChannels[0] = { metallic, 0.0f, false, ETextureChannelRole::Metallic };
            OutChannels[1] = { ao, 1.0f, false, ETextureChannelRole::AmbientOcclusion };
            OutChannels[2] = { LOCTEXT("DetailMask", "Detail Mask"), 0.0f, false, ETextureChannelRole::DetailMask };
            OutChannels[3] = { smoothness, 0.5f, true, ETextureChannelRole::Roughness };
            break;
        case EChannelPackPreset::MetallicSmoothness:
            OutChannels[0] = { metallic, 0.0f, false, ETextureChannelRole::Metallic };
            OutChannels[1] = { unused, 0.0f, false };
            OutChannels[2] = { unused, 0.0f, false };
            OutChannels[3] = { smoothness, 0.5f, true, ETextureChannelRole::Roughness };
            break;
        case EChannelPackPreset::Custom:
        default:
            OutChannels[0] = { LOCTEXT("Red", "Red"), 0.0f, false };
            OutChannels[1] = { LOCTEXT("Green", "Green"), 0.0f, false };
            OutChannels[2] = { LOCTEXT("Blue", "Blue"), 0.0f, false };
            OutChannels[3] = { LOCTEXT("Alpha", "Alpha"), 1.0f, false };
            break;
        }
    }

    FString MakeDefaultName(const UTexture2D* InTexture, const FString& InSuffix)
    {
        TArray<FString> tokens;
        InTexture->GetName().ParseIntoArray(tokens, TEXT("_"), true);
        if (tokens.Num() >= 3)
        {
            tokens.Pop();
        }
        tokens.Add(InSuffix);
        return FString::Join(tokens, TEXT("_"));
    }
}

void FTextureChannelPacker::ApplyPreset(EChannelPackPreset InPreset, TArray<FTextureChannelInput>& InOutInputs)
{
    TextureChannelPackerPrivate::FPresetChannel channels[4];
    TextureChannelPackerPrivate::GetPresetChannels(InPreset, channels);

    InOutInputs.SetNum(4);
    for (int32 channelIndex = 0; channelIndex < 4; ++channelIndex)
    {
        InOutInputs[channelIndex].Label = channels[channelIndex].Label;
        InOutInputs[channelIndex].ConstantValue = channels[channelIndex].ConstantValue;
        InOutInputs[channelIndex].bInvert = channels[channelIndex].bInvert;
        InOutInputs[channelIndex].Role = channels[channelIndex].Role;
    }
}

ETextureChannelRole FTextureChannelPacker::GuessRole(const FString& InTextureName)
{
    TArray<FString> tokens;
    InTextureName.ParseIntoArray(tokens, TEXT("_"), true);

    for (int32 tokenIndex = tokens.Num() - 1; tokenIndex >= 1; --tokenIndex)
    {
        const FString token = tokens[tokenIndex].ToLower();
        if (token == TEXT("ao") || token == TEXT("occ") || token == TEXT("occlusion") || token == TEXT("ambientocclusion"))
        {
            return ETextureChannelRole::AmbientOcclusion;
        }
        if (token == TEXT("rough") || token == TEXT("roughness") || token == TEXT("rgh"))
        {
            return ETextureChannelRole::Roughness;
        }
        if (token == TEXT("metal") || token == TEXT("metallic") || token == TEXT("metalness") || token == TEXT("mtl"))
        {
            return ETextureChannelRole::Metallic;
        }
        if (token == TEXT("smooth") || token == TEXT("smoothness") || token == TEXT("gloss") || token == TEXT("glossiness"))
        {
            return ETextureChannelRole::Smoothness;
        }
        if (token == TEXT("detail") || token == TEXT("detailmask"))
        {
            return ETextureChannelRole::DetailMask;
        }
    }
    return ETextureChannelRole::None;
}

void FTextureChannelPacker::AutoAssign(const TArray<UTexture2D*>& InTextures, TArray<FTextureChannelInput>& InOutInputs)
{
    InOutInputs.SetNum(4);

    TArray<UTexture2D*> unassigned;
    for (UTexture2D* texture : InTextures)
    {
        if (!texture)
        {
            continue;
        }

        const ETextureChannelRole role = GuessRole(texture->GetName());
        int32 targetIndex = INDEX_NONE;
        bool bFlipInvert = false;
        if (role != ETextureChannelRole::None)
        {
            targetIndex = InOutInputs.IndexOfByPredicate([role](const FTextureChannelInput& InInput)
            {
                return InInput.Role == role;
            });
            if (targetIndex == INDEX_NONE && role == ETextureChannelRole::Smoothness)
            {
                targetIndex = InOutInputs.IndexOfByPredicate([](const FTextureChannelInput& InInput)
                {
                    return InInput.Role == ETextureChannelRole::Roughness;
                });
                bFlipInvert = true;
            }
        }

        if (targetIndex == INDEX_NONE)
        {
            unassigned.Add(texture);
            continue;
        }

        FTextureChannelInput& input = InOutInputs[targetIndex];
        input.Texture = texture;
        input.SourceChannel = ETextureToolkitChannel::Red;
        if (bFlipInvert)
        {
            input.bInvert = !input.bInvert;
        }
    }

    for (UTexture2D* texture : unassigned)
    {
        const int32 emptyIndex = InOutInputs.IndexOfByPredicate([](const FTextureChannelInput& InInput)
        {
            return InInput.Texture == nullptr;
        });
        if (emptyIndex == INDEX_NONE)
        {
            break;
        }
        InOutInputs[emptyIndex].Texture = texture;
    }
}

bool FTextureChannelPacker::ResolveOutput(const TArray<FTextureChannelInput>& InInputs, EChannelPackPreset InPreset, const FTextureChannelPackOptions& InOptions, FString& OutFolder, FString& OutName)
{
    UTexture2D* firstTexture = GetFirstTexture(InInputs);
    if (!firstTexture)
    {
        return false;
    }

    OutFolder = FTextureToolkitAssetUtils::NormalizeFolder(InOptions.OutputFolder.Path);
    if (OutFolder.IsEmpty())
    {
        OutFolder = FTextureToolkitAssetUtils::GetAssetFolder(firstTexture);
    }

    OutName = InOptions.OutputName.TrimStartAndEnd();
    if (OutName.IsEmpty())
    {
        OutName = TextureChannelPackerPrivate::MakeDefaultName(firstTexture, GetPresetSuffix(InPreset));
    }
    return true;
}

FString FTextureChannelPacker::GetPresetSuffix(EChannelPackPreset InPreset)
{
    switch (InPreset)
    {
    case EChannelPackPreset::ORM:
        return TEXT("ORM");
    case EChannelPackPreset::RMA:
        return TEXT("RMA");
    case EChannelPackPreset::MRA:
        return TEXT("MRA");
    case EChannelPackPreset::MaskMap:
        return TEXT("Mask");
    case EChannelPackPreset::MetallicSmoothness:
        return TEXT("MS");
    default:
        return TEXT("Packed");
    }
}

UTexture2D* FTextureChannelPacker::GetFirstTexture(const TArray<FTextureChannelInput>& InInputs)
{
    for (const FTextureChannelInput& input : InInputs)
    {
        if (input.Texture)
        {
            return input.Texture;
        }
    }
    return nullptr;
}

FIntPoint FTextureChannelPacker::ComputeOutputSize(const TArray<FTextureChannelInput>& InInputs, const FTextureChannelPackOptions& InOptions)
{
    const FIntPoint customSize(TextureToolkitSizeToPixels(InOptions.CustomWidth), TextureToolkitSizeToPixels(InOptions.CustomHeight));
    if (InOptions.SizeMode == EChannelPackSizeMode::Custom)
    {
        return customSize;
    }

    bool bFound = false;
    FIntPoint result = FIntPoint::ZeroValue;
    for (const FTextureChannelInput& input : InInputs)
    {
        if (!FTextureToolkitImageUtils::CanReadSource(input.Texture))
        {
            continue;
        }

        const FIntPoint size(static_cast<int32>(input.Texture->Source.GetSizeX()), static_cast<int32>(input.Texture->Source.GetSizeY()));
        if (!bFound)
        {
            result = size;
            bFound = true;
        }
        else if (InOptions.SizeMode == EChannelPackSizeMode::LargestInput)
        {
            result = result.ComponentMax(size);
        }
        else
        {
            result = result.ComponentMin(size);
        }
    }

    if (!bFound)
    {
        return customSize;
    }
    return FIntPoint(FMath::Max(1, result.X), FMath::Max(1, result.Y));
}

bool FTextureChannelPacker::Pack(const TArray<FTextureChannelInput>& InInputs, const FIntPoint& InSize, FTextureChannelImageCache* InOutCache, FImage& OutImage)
{
    if (InInputs.Num() < 4 || InSize.X <= 0 || InSize.Y <= 0)
    {
        return false;
    }

    FTextureChannelImageCache localCache;
    FTextureChannelImageCache& images = InOutCache ? *InOutCache : localCache;

    for (const FTextureChannelInput& input : InInputs)
    {
        UTexture2D* texture = input.Texture;
        if (!texture)
        {
            continue;
        }

        const FImage* cached = images.Find(texture);
        if (cached && cached->SizeX == InSize.X && cached->SizeY == InSize.Y)
        {
            continue;
        }

        FImage rawImage;
        if (!FTextureToolkitImageUtils::ReadRawImage(texture, rawImage))
        {
            return false;
        }

        FImage resizedImage;
        if (rawImage.SizeX == InSize.X && rawImage.SizeY == InSize.Y)
        {
            resizedImage = MoveTemp(rawImage);
        }
        else
        {
            FImageCore::ResizeImageAllocDest(rawImage, resizedImage, InSize.X, InSize.Y);
        }
        images.Add(texture, MoveTemp(resizedImage));
    }

    const FLinearColor* sources[4] = { nullptr, nullptr, nullptr, nullptr };
    for (int32 channelIndex = 0; channelIndex < 4; ++channelIndex)
    {
        if (UTexture2D* texture = InInputs[channelIndex].Texture)
        {
            if (const FImage* image = images.Find(texture))
            {
                sources[channelIndex] = image->AsRGBA32F().GetData();
            }
        }
    }

    OutImage.Init(InSize.X, InSize.Y, ERawImageFormat::RGBA32F, EGammaSpace::Linear);
    const TArrayView64<FLinearColor> outPixels = OutImage.AsRGBA32F();
    const int64 numPixels = OutImage.GetNumPixels();

    for (int64 pixelIndex = 0; pixelIndex < numPixels; ++pixelIndex)
    {
        FLinearColor& outColor = outPixels[pixelIndex];
        for (int32 channelIndex = 0; channelIndex < 4; ++channelIndex)
        {
            const FTextureChannelInput& input = InInputs[channelIndex];
            float value = input.ConstantValue;
            if (sources[channelIndex])
            {
                value = FMath::Clamp(FTextureToolkitImageUtils::GetChannelValue(sources[channelIndex][pixelIndex], input.SourceChannel), 0.0f, 1.0f);
                if (input.bInvert)
                {
                    value = 1.0f - value;
                }
            }
            outColor.Component(channelIndex) = value;
        }
    }
    return true;
}

UTexture2D* FTextureChannelPacker::CreatePackedTexture(const TArray<FTextureChannelInput>& InInputs, EChannelPackPreset InPreset, const FTextureChannelPackOptions& InOptions, FText& OutError)
{
    FString folder;
    FString assetName;
    if (!ResolveOutput(InInputs, InPreset, InOptions, folder, assetName))
    {
        OutError = LOCTEXT("NoInputs", "Assign at least one input texture.");
        return nullptr;
    }
    UTexture2D* firstTexture = GetFirstTexture(InInputs);

    const FIntPoint outputSize = ComputeOutputSize(InInputs, InOptions);

    FScopedSlowTask slowTask(3.0f, FText::Format(LOCTEXT("Packing", "Packing channels into {0}..."), FText::FromString(assetName)));
    slowTask.MakeDialog();

    slowTask.EnterProgressFrame(2.0f);
    FImage packedImage;
    if (!Pack(InInputs, outputSize, nullptr, packedImage))
    {
        OutError = LOCTEXT("PackFailed", "Failed to read one of the input textures. Check the Output Log.");
        return nullptr;
    }

    slowTask.EnterProgressFrame(1.0f);
    FImage outputImage;
    const ERawImageFormat::Type outputFormat = InOptions.BitDepth == EChannelPackBitDepth::Bit16 ? ERawImageFormat::RGBA16 : ERawImageFormat::BGRA8;
    packedImage.CopyTo(outputImage, outputFormat, EGammaSpace::Linear);

    FTextureAssetSettings settings;
    settings.CompressionSettings = InOptions.CompressionSettings;
    settings.bSRGB = InOptions.bSRGB;
    settings.LODGroup = firstTexture->LODGroup;
    settings.MipGenSettings = TMGS_FromTextureGroup;

    UTexture2D* result = FTextureToolkitAssetUtils::CreateOrUpdateTexture(folder, assetName, outputImage, settings, OutError);
    if (result && InOptions.bSaveAssets)
    {
        FTextureToolkitAssetUtils::SaveAssets({ result });
    }
    return result;
}

bool FTextureChannelPacker::SplitChannels(UTexture2D* InTexture, uint8 InChannelMask, const FString& InFolder, bool bInSave, TArray<UTexture2D*>& OutTextures, FText& OutError)
{
    if (!InTexture)
    {
        OutError = LOCTEXT("NoSplitTexture", "Pick a texture to split.");
        return false;
    }
    if ((InChannelMask & 0xF) == 0)
    {
        OutError = LOCTEXT("NoSplitChannels", "Select at least one channel to export.");
        return false;
    }

    FScopedSlowTask slowTask(5.0f, FText::Format(LOCTEXT("Splitting", "Splitting {0}..."), FText::FromString(InTexture->GetName())));
    slowTask.MakeDialog();
    slowTask.EnterProgressFrame(1.0f);

    FImage rawImage;
    if (!FTextureToolkitImageUtils::ReadRawImage(InTexture, rawImage))
    {
        OutError = FText::Format(LOCTEXT("SplitReadFailed", "Failed to read {0}."), FText::FromString(InTexture->GetName()));
        return false;
    }

    FString folder = FTextureToolkitAssetUtils::NormalizeFolder(InFolder);
    if (folder.IsEmpty())
    {
        folder = FTextureToolkitAssetUtils::GetAssetFolder(InTexture);
    }

    static const TCHAR* channelSuffixes[4] = { TEXT("R"), TEXT("G"), TEXT("B"), TEXT("A") };
    const TArrayView64<const FLinearColor> sourcePixels = static_cast<const FImage&>(rawImage).AsRGBA32F();
    const int64 numPixels = rawImage.GetNumPixels();

    FTextureAssetSettings settings;
    settings.CompressionSettings = TC_Grayscale;
    settings.bSRGB = false;
    settings.LODGroup = InTexture->LODGroup;
    settings.MipGenSettings = InTexture->MipGenSettings;

    bool bAllWritten = true;
    for (int32 channelIndex = 0; channelIndex < 4; ++channelIndex)
    {
        slowTask.EnterProgressFrame(1.0f);
        if ((InChannelMask & (1 << channelIndex)) == 0)
        {
            continue;
        }

        FImage grayImage;
        grayImage.Init(rawImage.SizeX, rawImage.SizeY, ERawImageFormat::G8, EGammaSpace::Linear);
        const TArrayView64<uint8> grayPixels = grayImage.AsG8();
        for (int64 pixelIndex = 0; pixelIndex < numPixels; ++pixelIndex)
        {
            const float value = sourcePixels[pixelIndex].Component(channelIndex);
            grayPixels[pixelIndex] = static_cast<uint8>(FMath::Clamp(FMath::RoundToInt(value * 255.0f), 0, 255));
        }

        FText channelError;
        const FString assetName = FString::Printf(TEXT("%s_%s"), *InTexture->GetName(), channelSuffixes[channelIndex]);
        if (UTexture2D* channelTexture = FTextureToolkitAssetUtils::CreateOrUpdateTexture(folder, assetName, grayImage, settings, channelError))
        {
            OutTextures.Add(channelTexture);
        }
        else
        {
            OutError = channelError;
            bAllWritten = false;
        }
    }

    if (bInSave && OutTextures.Num() > 0)
    {
        TArray<UObject*> assets(OutTextures);
        FTextureToolkitAssetUtils::SaveAssets(assets);
    }
    return bAllWritten;
}

#undef LOCTEXT_NAMESPACE
