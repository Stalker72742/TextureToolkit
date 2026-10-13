// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"

#include "Core/TextureToolkitTypes.h"

class UTexture2D;
class UPaperSprite;

/**
 * @brief Everything needed to write an atlas.
 */
struct FTextureAtlasBuildInput
{
    /** @brief Final atlas size. */
    FIntPoint AtlasSize = FIntPoint::ZeroValue;

    /** @brief Layout and output options. */
    FTextureAtlasOptions Options;

    /** @brief Placed sprites. */
    TArray<TSharedPtr<FTextureAtlasItem>> Items;

    /** @brief Existing atlas whose pixels are kept at the top left corner. */
    UTexture2D* BaseAtlas = nullptr;

    /** @brief Size of the base atlas before the build. */
    FIntPoint BaseAtlasSize = FIntPoint::ZeroValue;

    /** @brief Sprites that already use the base atlas. Refreshed when the atlas size changes. */
    TArray<UPaperSprite*> BaseSprites;
};

/**
 * @brief Output of an atlas build.
 */
struct FTextureAtlasBuildResult
{
    /** @brief Created or updated atlas texture. */
    UTexture2D* Atlas = nullptr;

    /** @brief Source texture to generated sprite. */
    TMap<UTexture2D*, UPaperSprite*> Sprites;

    /** @brief Every asset written by the build. */
    TArray<UObject*> ModifiedAssets;
};

/**
 * @brief Writes atlas textures and sprites, analyzes existing atlases.
 */
class FTextureAtlasBuilder
{
public:
    /**
     * @brief Collects regions that are already used in an existing atlas.
     * @param InAtlas Existing atlas texture.
     * @param InAlphaThreshold Alpha values at or below this are treated as empty.
     * @param OutOccupied Sprite rects plus merged blocks of non transparent pixels.
     * @param OutSprites Sprites using the atlas.
     */
    static void AnalyzeBaseAtlas(UTexture2D* InAtlas, uint8 InAlphaThreshold, TArray<FIntRect>& OutOccupied, TArray<UPaperSprite*>& OutSprites);

    /**
     * @brief Computes the opaque bounds of an item texture.
     * @param InOutItem Item to update.
     * @param InAlphaThreshold Alpha values at or below this are transparent.
     * @return False when the texture cannot be read.
     */
    static bool ComputeOpaqueBounds(FTextureAtlasItem& InOutItem, uint8 InAlphaThreshold);

    /**
     * @brief Writes the atlas texture and the sprites.
     * @param InInput Build input.
     * @param OutResult Written assets.
     * @param OutError Error text on failure.
     * @return True on success.
     */
    static bool Build(const FTextureAtlasBuildInput& InInput, FTextureAtlasBuildResult& OutResult, FText& OutError);

    /**
     * @brief Returns the default sprite name for a texture.
     * @param InPrefix Sprite prefix.
     * @param InTexture Source texture.
     * @return Prefix followed by the texture name without "T_".
     */
    static FString MakeSpriteName(const FString& InPrefix, const UTexture2D* InTexture);

    /**
     * @brief Returns the folder used for the atlas texture.
     * @param InInput Build input.
     * @return Normalized folder.
     */
    static FString ResolveAtlasFolder(const FTextureAtlasBuildInput& InInput);
};
