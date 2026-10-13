// Fill out your copyright notice in the Description page of Project Settings.

#include "Core/TextureCompressor.h"

#include "Engine/Texture2D.h"
#include "ImageCore.h"

#include "Core/TextureToolkitImageUtils.h"

#define LOCTEXT_NAMESPACE "TextureCompressor"

namespace TextureCompressorPrivate
{
    FIntPoint HalveTimes(const FIntPoint& InSize, int32 InSteps)
    {
        FIntPoint result = InSize;
        for (int32 step = 0; step < InSteps && (result.X > 1 || result.Y > 1); ++step)
        {
            result.X = FMath::Max(1, result.X / 2);
            result.Y = FMath::Max(1, result.Y / 2);
        }
        return result;
    }

    FText SizeText(const FIntPoint& InSize)
    {
        return FText::Format(LOCTEXT("SizeFormat", "{0}x{1}"), FText::AsNumber(InSize.X), FText::AsNumber(InSize.Y));
    }
}

FIntPoint FTextureCompressor::GetBuiltSize(UTexture2D* InTexture)
{
    if (!InTexture->IsCompiling() && InTexture->GetSizeX() > 0 && InTexture->GetSizeY() > 0)
    {
        return FIntPoint(InTexture->GetSizeX(), InTexture->GetSizeY());
    }

    const FIntPoint sourceSize(static_cast<int32>(InTexture->Source.GetSizeX()), static_cast<int32>(InTexture->Source.GetSizeY()));
    return FTextureToolkitImageUtils::HalveToFit(sourceSize, InTexture->MaxTextureSize);
}

FTextureCompressPlan FTextureCompressor::BuildPlan(UTexture2D* InTexture, const FTextureCompressOptions& InOptions)
{
    FTextureCompressPlan plan;

    FText reason;
    if (!FTextureToolkitImageUtils::CanReadSource(InTexture, &reason))
    {
        plan.bInvalid = true;
        plan.Note = reason;
        return plan;
    }

    const bool bCompiling = InTexture->IsCompiling();
    const bool bHasAlpha = bCompiling ? false : InTexture->HasAlphaChannel();

    plan.SourceSize = FIntPoint(static_cast<int32>(InTexture->Source.GetSizeX()), static_cast<int32>(InTexture->Source.GetSizeY()));
    const FIntPoint builtSize = GetBuiltSize(InTexture);
    const int32 currentBias = FMath::Max(0, InTexture->LODBias);
    plan.CurrentSize = TextureCompressorPrivate::HalveTimes(builtSize, currentBias);

    plan.CurrentFormat = bCompiling ? PF_Unknown : InTexture->GetPixelFormat();
    if (plan.CurrentFormat == PF_Unknown)
    {
        plan.CurrentFormat = FTextureToolkitImageUtils::GuessPixelFormat(InTexture->CompressionSettings, bHasAlpha);
    }

    const bool bCurrentMips = (!bCompiling && InTexture->GetNumMips() > 0) ? InTexture->GetNumMips() > 1 : InTexture->MipGenSettings != TMGS_NoMipmaps;
    plan.CurrentBytes = FTextureToolkitImageUtils::EstimateMemoryBytes(plan.CurrentSize, plan.CurrentFormat, bCurrentMips);

    plan.NewSourceSize = plan.SourceSize;
    plan.NewMaxTextureSize = InTexture->MaxTextureSize;
    plan.NewLODBias = InTexture->LODBias;
    plan.NewSize = plan.CurrentSize;
    plan.NewFormat = plan.CurrentFormat;
    plan.NewBytes = plan.CurrentBytes;

    const bool bByDimension = InOptions.ResizeRule == ETextureResizeRule::MaxDimension;
    const FIntPoint referenceSize = InOptions.Method == ETextureCompressMethod::ResizeSource ? plan.SourceSize : builtSize;
    const int32 referenceMax = FMath::Max(referenceSize.X, referenceSize.Y);
    const int32 targetMax = bByDimension
        ? TextureToolkitSizeToPixels(InOptions.TargetMaxSize)
        : FMath::Max(1, referenceMax / TextureScaleFactorToDivider(InOptions.ScaleFactor));
    const bool bResize = referenceMax > targetMax;

    if (bByDimension && InOptions.bOnlyLargerTextures && !bResize)
    {
        plan.Note = LOCTEXT("AlreadyFits", "Already fits");
        return plan;
    }

    FIntPoint newBuiltSize = builtSize;
    if (bResize)
    {
        switch (InOptions.Method)
        {
        case ETextureCompressMethod::MaxInGameSize:
            plan.NewMaxTextureSize = targetMax;
            newBuiltSize = FTextureToolkitImageUtils::HalveToFit(builtSize, targetMax);
            break;

        case ETextureCompressMethod::LODBias:
        {
            if (bByDimension)
            {
                int32 steps = 0;
                FTextureToolkitImageUtils::HalveToFit(builtSize, targetMax, &steps);
                plan.NewLODBias = FMath::Max(currentBias, steps);
            }
            else
            {
                plan.NewLODBias = currentBias + static_cast<int32>(FMath::FloorLog2(static_cast<uint32>(TextureScaleFactorToDivider(InOptions.ScaleFactor))));
            }
            break;
        }

        case ETextureCompressMethod::ResizeSource:
            plan.NewSourceSize = FTextureToolkitImageUtils::ScaleToFit(plan.SourceSize, targetMax);
            newBuiltSize = FTextureToolkitImageUtils::HalveToFit(plan.NewSourceSize, InTexture->MaxTextureSize);
            break;

        default:
            break;
        }
    }
    plan.NewSize = TextureCompressorPrivate::HalveTimes(newBuiltSize, FMath::Max(0, plan.NewLODBias));

    const bool bNewMips = InOptions.bOverrideMipGen ? InOptions.MipGenSettings != TMGS_NoMipmaps : bCurrentMips;
    plan.NewFormat = InOptions.bOverrideCompression
        ? FTextureToolkitImageUtils::GuessPixelFormat(InOptions.CompressionSettings, bHasAlpha)
        : plan.CurrentFormat;
    plan.NewBytes = FTextureToolkitImageUtils::EstimateMemoryBytes(plan.NewSize, plan.NewFormat, bNewMips);

    const bool bSizeChanged = plan.NewSourceSize != plan.SourceSize
        || plan.NewMaxTextureSize != InTexture->MaxTextureSize
        || plan.NewLODBias != InTexture->LODBias;

    const bool bFormatChanged = (InOptions.bOverrideCompression && InOptions.CompressionSettings != InTexture->CompressionSettings)
        || (InOptions.bOverrideSRGB && InOptions.bSRGB != static_cast<bool>(InTexture->SRGB))
        || (InOptions.bOverrideMipGen && InOptions.MipGenSettings != InTexture->MipGenSettings)
        || (InOptions.bOverrideLODGroup && InOptions.LODGroup != InTexture->LODGroup);

    plan.bChanged = bSizeChanged || bFormatChanged;

    TArray<FText> notes;
    if (plan.NewSourceSize != plan.SourceSize)
    {
        notes.Add(FText::Format(LOCTEXT("NoteResize", "Resize source to {0}"), TextureCompressorPrivate::SizeText(plan.NewSourceSize)));
    }
    if (plan.NewMaxTextureSize != InTexture->MaxTextureSize)
    {
        notes.Add(FText::Format(LOCTEXT("NoteMaxSize", "Max size {0}"), FText::AsNumber(plan.NewMaxTextureSize)));
    }
    if (plan.NewLODBias != InTexture->LODBias)
    {
        notes.Add(FText::Format(LOCTEXT("NoteLODBias", "LOD bias {0}"), FText::AsNumber(plan.NewLODBias)));
    }
    if (bFormatChanged)
    {
        notes.Add(LOCTEXT("NoteFormat", "Format settings"));
    }
    if (bCompiling)
    {
        notes.Add(LOCTEXT("NoteCompiling", "compiling, estimate"));
    }

    plan.Note = notes.Num() > 0 ? FText::Join(LOCTEXT("NoteSeparator", ", "), notes) : LOCTEXT("NoChanges", "No changes");
    return plan;
}

bool FTextureCompressor::ApplyPlan(UTexture2D* InTexture, const FTextureCompressPlan& InPlan, const FTextureCompressOptions& InOptions, FText& OutError)
{
    if (!InTexture || InPlan.bInvalid || !InPlan.bChanged)
    {
        return false;
    }

    FImage resizedImage;
    const bool bResizeSource = InPlan.NewSourceSize != InPlan.SourceSize;
    if (bResizeSource)
    {
        FImage sourceImage;
        if (!FTextureToolkitImageUtils::ReadSourceImage(InTexture, sourceImage))
        {
            OutError = FText::Format(LOCTEXT("ReadFailed", "Failed to read source of {0}."), FText::FromString(InTexture->GetName()));
            return false;
        }
        FImageCore::ResizeImageAllocDest(sourceImage, resizedImage, InPlan.NewSourceSize.X, InPlan.NewSourceSize.Y);
    }

    InTexture->Modify();
    InTexture->PreEditChange(nullptr);

    if (bResizeSource)
    {
        InTexture->Source.Init(resizedImage);
    }

    InTexture->MaxTextureSize = InPlan.NewMaxTextureSize;
    InTexture->LODBias = InPlan.NewLODBias;

    if (InOptions.bOverrideCompression)
    {
        InTexture->CompressionSettings = InOptions.CompressionSettings;
    }
    if (InOptions.bOverrideSRGB)
    {
        InTexture->SRGB = InOptions.bSRGB;
    }
    if (InOptions.bOverrideMipGen)
    {
        InTexture->MipGenSettings = InOptions.MipGenSettings;
    }
    if (InOptions.bOverrideLODGroup)
    {
        InTexture->LODGroup = InOptions.LODGroup;
    }

    InTexture->PostEditChange();
    InTexture->MarkPackageDirty();
    return true;
}

#undef LOCTEXT_NAMESPACE
