// Fill out your copyright notice in the Description page of Project Settings.

#include "Core/TextureAtlasBuilder.h"

#include "Engine/Texture2D.h"
#include "ImageCore.h"
#include "Misc/ScopedSlowTask.h"
#include "PaperSprite.h"
#include "SpriteEditorOnlyTypes.h"

#include "Core/TextureToolkitAssetUtils.h"
#include "Core/TextureToolkitImageUtils.h"
#include "TextureToolkitLog.h"

#define LOCTEXT_NAMESPACE "TextureAtlasBuilder"

namespace TextureAtlasBuilderPrivate
{
    FIntRect GetSpriteRect(const UPaperSprite* InSprite)
    {
        const FVector2D sourceUV = InSprite->GetSourceUV();
        const FVector2D sourceSize = InSprite->GetSourceSize();
        const FIntPoint minPoint(FMath::RoundToInt(sourceUV.X), FMath::RoundToInt(sourceUV.Y));
        return FIntRect(minPoint, minPoint + FIntPoint(FMath::RoundToInt(sourceSize.X), FMath::RoundToInt(sourceSize.Y)));
    }

    void MergeOccupiedBlocks(const TArray<uint8>& InBlocks, int32 InBlocksX, int32 InBlocksY, TArray<FIntRect>& OutRects)
    {
        TArray<FIntRect> openRects;
        for (int32 blockY = 0; blockY < InBlocksY; ++blockY)
        {
            TArray<FIntRect> nextOpenRects;
            int32 blockX = 0;
            while (blockX < InBlocksX)
            {
                if (!InBlocks[blockY * InBlocksX + blockX])
                {
                    ++blockX;
                    continue;
                }

                const int32 runStart = blockX;
                while (blockX < InBlocksX && InBlocks[blockY * InBlocksX + blockX])
                {
                    ++blockX;
                }

                const int32 openIndex = openRects.IndexOfByPredicate([runStart, blockX](const FIntRect& InRect)
                {
                    return InRect.Min.X == runStart && InRect.Max.X == blockX;
                });

                if (openIndex != INDEX_NONE)
                {
                    FIntRect extended = openRects[openIndex];
                    extended.Max.Y = blockY + 1;
                    nextOpenRects.Add(extended);
                    openRects.RemoveAtSwap(openIndex);
                }
                else
                {
                    nextOpenRects.Add(FIntRect(runStart, blockY, blockX, blockY + 1));
                }
            }

            OutRects.Append(openRects);
            openRects = MoveTemp(nextOpenRects);
        }
        OutRects.Append(openRects);
    }
}

void FTextureAtlasBuilder::AnalyzeBaseAtlas(UTexture2D* InAtlas, uint8 InAlphaThreshold, TArray<FIntRect>& OutOccupied, TArray<UPaperSprite*>& OutSprites)
{
    OutOccupied.Reset();
    OutSprites.Reset();
    if (!InAtlas)
    {
        return;
    }

    FTextureToolkitAssetUtils::FindSpritesUsingTexture(InAtlas, OutSprites);
    for (const UPaperSprite* sprite : OutSprites)
    {
        const FIntRect spriteRect = TextureAtlasBuilderPrivate::GetSpriteRect(sprite);
        if (spriteRect.Width() > 0 && spriteRect.Height() > 0)
        {
            OutOccupied.Add(spriteRect);
        }
    }

    FImage atlasImage;
    if (!FTextureToolkitImageUtils::ReadDisplayImage(InAtlas, atlasImage))
    {
        return;
    }

    const int32 blockSize = FMath::Clamp(FMath::Max(atlasImage.SizeX, atlasImage.SizeY) / 256, 4, 16);
    const int32 blocksX = FMath::DivideAndRoundUp(atlasImage.SizeX, blockSize);
    const int32 blocksY = FMath::DivideAndRoundUp(atlasImage.SizeY, blockSize);

    TArray<uint8> blocks;
    blocks.SetNumZeroed(blocksX * blocksY);

    const TArrayView64<const FColor> pixels = static_cast<const FImage&>(atlasImage).AsBGRA8();
    for (int32 y = 0; y < atlasImage.SizeY; ++y)
    {
        const int64 rowOffset = static_cast<int64>(y) * atlasImage.SizeX;
        const int32 blockRow = (y / blockSize) * blocksX;
        for (int32 x = 0; x < atlasImage.SizeX; ++x)
        {
            if (pixels[rowOffset + x].A > InAlphaThreshold)
            {
                blocks[blockRow + x / blockSize] = 1;
            }
        }
    }

    TArray<FIntRect> blockRects;
    TextureAtlasBuilderPrivate::MergeOccupiedBlocks(blocks, blocksX, blocksY, blockRects);

    const FIntPoint atlasSize(atlasImage.SizeX, atlasImage.SizeY);
    for (const FIntRect& blockRect : blockRects)
    {
        OutOccupied.Add(FIntRect(blockRect.Min * blockSize, (blockRect.Max * blockSize).ComponentMin(atlasSize)));
    }
}

bool FTextureAtlasBuilder::ComputeOpaqueBounds(FTextureAtlasItem& InOutItem, uint8 InAlphaThreshold)
{
    FImage image;
    if (!FTextureToolkitImageUtils::ReadDisplayImage(InOutItem.Texture, image))
    {
        return false;
    }

    InOutItem.SourceSize = FIntPoint(image.SizeX, image.SizeY);
    InOutItem.OpaqueBounds = FTextureToolkitImageUtils::ComputeOpaqueBounds(image, InAlphaThreshold);
    InOutItem.bOpaqueBoundsValid = true;
    return true;
}

FString FTextureAtlasBuilder::MakeSpriteName(const FString& InPrefix, const UTexture2D* InTexture)
{
    return InPrefix + FTextureToolkitAssetUtils::GetBaseName(GetNameSafe(InTexture));
}

FString FTextureAtlasBuilder::ResolveAtlasFolder(const FTextureAtlasBuildInput& InInput)
{
    FString folder = FTextureToolkitAssetUtils::NormalizeFolder(InInput.Options.OutputFolder.Path);
    if (folder.IsEmpty() && InInput.BaseAtlas)
    {
        folder = FTextureToolkitAssetUtils::GetAssetFolder(InInput.BaseAtlas);
    }
    if (folder.IsEmpty())
    {
        for (const TSharedPtr<FTextureAtlasItem>& item : InInput.Items)
        {
            if (item.IsValid() && item->Texture)
            {
                folder = FTextureToolkitAssetUtils::GetAssetFolder(item->Texture);
                break;
            }
        }
    }
    return folder;
}

bool FTextureAtlasBuilder::Build(const FTextureAtlasBuildInput& InInput, FTextureAtlasBuildResult& OutResult, FText& OutError)
{
    const FTextureAtlasOptions& options = InInput.Options;
    if (InInput.AtlasSize.X <= 0 || InInput.AtlasSize.Y <= 0)
    {
        OutError = LOCTEXT("InvalidSize", "Atlas size is invalid.");
        return false;
    }

    const FString folder = ResolveAtlasFolder(InInput);
    if (folder.IsEmpty())
    {
        OutError = LOCTEXT("NoFolder", "Set an output folder for the atlas.");
        return false;
    }

    FString atlasName = options.AtlasName.TrimStartAndEnd();
    if (atlasName.IsEmpty())
    {
        atlasName = InInput.BaseAtlas ? InInput.BaseAtlas->GetName() : FString(TEXT("T_Atlas"));
    }

    FScopedSlowTask slowTask(static_cast<float>(InInput.Items.Num() + 4), FText::Format(LOCTEXT("Building", "Building atlas {0}..."), FText::FromString(atlasName)));
    slowTask.MakeDialog(true);

    FImage atlasImage;
    atlasImage.Init(InInput.AtlasSize.X, InInput.AtlasSize.Y, ERawImageFormat::BGRA8, EGammaSpace::sRGB);
    FMemory::Memzero(atlasImage.RawData.GetData(), atlasImage.RawData.Num());

    slowTask.EnterProgressFrame(1.0f, LOCTEXT("CopyBase", "Copying base atlas"));
    if (InInput.BaseAtlas)
    {
        FImage baseImage;
        if (FTextureToolkitImageUtils::ReadDisplayImage(InInput.BaseAtlas, baseImage))
        {
            FTextureToolkitImageUtils::BlitImage(baseImage, atlasImage, FIntPoint::ZeroValue);
        }
    }

    const int32 extrudeAmount = options.bExtrudeEdges ? options.Padding : 0;
    for (const TSharedPtr<FTextureAtlasItem>& item : InInput.Items)
    {
        if (!item.IsValid() || !item->Texture)
        {
            slowTask.EnterProgressFrame(1.0f);
            continue;
        }

        slowTask.EnterProgressFrame(1.0f, FText::FromString(item->Texture->GetName()));
        if (slowTask.ShouldCancel())
        {
            OutError = LOCTEXT("Cancelled", "Atlas build was cancelled.");
            return false;
        }

        FImage sourceImage;
        if (!FTextureToolkitImageUtils::ReadDisplayImage(item->Texture, sourceImage))
        {
            UE_LOG(LogTextureToolkit, Warning, TEXT("[FTextureAtlasBuilder] Skipped %s: source cannot be read."), *item->Texture->GetPathName());
            continue;
        }

        const FIntRect sourceRect = item->GetSourceRect();
        FImage spriteImage;
        if (sourceRect.Min == FIntPoint::ZeroValue && sourceRect.Max == FIntPoint(sourceImage.SizeX, sourceImage.SizeY))
        {
            spriteImage = MoveTemp(sourceImage);
        }
        else
        {
            FTextureToolkitImageUtils::CropImage(sourceImage, sourceRect, spriteImage);
        }

        const FIntPoint spriteSize = item->GetSize();
        if (spriteImage.SizeX != spriteSize.X || spriteImage.SizeY != spriteSize.Y)
        {
            FImage scaledImage;
            FImageCore::ResizeImageAllocDest(spriteImage, scaledImage, spriteSize.X, spriteSize.Y,
                FImageCore::EResizeImageFilter::Default | FImageCore::EResizeImageFilter::Flag_AlphaWeighted);
            spriteImage = MoveTemp(scaledImage);
        }

        FTextureToolkitImageUtils::BlitImage(spriteImage, atlasImage, item->Position);
        if (extrudeAmount > 0)
        {
            FTextureToolkitImageUtils::ExtrudeEdges(atlasImage, item->GetRect(), extrudeAmount);
        }
    }

    slowTask.EnterProgressFrame(1.0f, LOCTEXT("WriteAtlas", "Writing atlas texture"));
    FTextureAssetSettings settings;
    settings.CompressionSettings = options.CompressionSettings;
    settings.LODGroup = options.LODGroup;
    settings.MipGenSettings = options.MipGenSettings;
    settings.bSRGB = options.bSRGB;

    UTexture2D* atlas = FTextureToolkitAssetUtils::CreateOrUpdateTexture(folder, atlasName, atlasImage, settings, OutError);
    if (!atlas)
    {
        return false;
    }
    OutResult.Atlas = atlas;
    OutResult.ModifiedAssets.Add(atlas);

    slowTask.EnterProgressFrame(1.0f, LOCTEXT("RefreshSprites", "Refreshing existing sprites"));
    if (atlas == InInput.BaseAtlas && InInput.BaseAtlasSize != InInput.AtlasSize)
    {
        for (UPaperSprite* sprite : InInput.BaseSprites)
        {
            if (!IsValid(sprite) || sprite->GetSourceTexture() != atlas)
            {
                continue;
            }

            const FIntRect spriteRect = TextureAtlasBuilderPrivate::GetSpriteRect(sprite);
            FSpriteAssetInitParameters initParams;
            initParams.Texture = atlas;
            initParams.Offset = spriteRect.Min;
            initParams.Dimension = spriteRect.Size();

            sprite->Modify();
            sprite->InitializeSprite(initParams);
            sprite->PostEditChange();
            sprite->MarkPackageDirty();
            OutResult.ModifiedAssets.Add(sprite);
        }
    }

    slowTask.EnterProgressFrame(1.0f, LOCTEXT("WriteSprites", "Writing sprites"));
    if (options.bCreateSprites)
    {
        FString spriteFolder = FTextureToolkitAssetUtils::NormalizeFolder(options.SpriteFolder.Path);
        if (spriteFolder.IsEmpty())
        {
            spriteFolder = folder / TEXT("Sprites");
        }

        TSet<FString> usedNames;
        for (const TSharedPtr<FTextureAtlasItem>& item : InInput.Items)
        {
            if (!item.IsValid() || !item->Texture)
            {
                continue;
            }

            const FString baseName = MakeSpriteName(options.SpritePrefix, item->Texture);
            FString spriteName = baseName;
            for (int32 suffix = 2; usedNames.Contains(spriteName); ++suffix)
            {
                spriteName = FString::Printf(TEXT("%s_%d"), *baseName, suffix);
            }
            usedNames.Add(spriteName);

            FText spriteError;
            UPaperSprite* sprite = FTextureToolkitAssetUtils::CreateOrUpdateSprite(spriteFolder, spriteName, atlas, item->GetRect(), spriteError);
            if (!sprite)
            {
                UE_LOG(LogTextureToolkit, Warning, TEXT("[FTextureAtlasBuilder] Sprite %s was not written: %s"), *spriteName, *spriteError.ToString());
                continue;
            }

            if (item->bTrimmed && item->bOpaqueBoundsValid)
            {
                const FVector2D origin(item->OpaqueBounds.Min.X * item->Scale, item->OpaqueBounds.Min.Y * item->Scale);
                const FVector2D fullSize(item->SourceSize.X * item->Scale, item->SourceSize.Y * item->Scale);
                sprite->SetTrim(true, origin, fullSize);
            }

            OutResult.Sprites.Add(item->Texture, sprite);
            OutResult.ModifiedAssets.Add(sprite);
        }
    }

    if (options.bSaveAssets)
    {
        FTextureToolkitAssetUtils::SaveAssets(OutResult.ModifiedAssets);
    }
    return true;
}

#undef LOCTEXT_NAMESPACE
