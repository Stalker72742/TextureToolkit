// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/TextureDefines.h"

class UTexture2D;
class UPaperSprite;
struct FImage;

/**
 * @brief Settings written to a texture created or updated by the toolkit.
 */
struct FTextureAssetSettings
{
    /** @brief Compression settings. */
    TextureCompressionSettings CompressionSettings = TC_Default;

    /** @brief Texture group. */
    TextureGroup LODGroup = TEXTUREGROUP_World;

    /** @brief Mip generation. */
    TextureMipGenSettings MipGenSettings = TMGS_FromTextureGroup;

    /** @brief sRGB flag. */
    bool bSRGB = true;
};

/**
 * @brief Result of a brush reference replacement pass.
 */
struct FTextureReplaceReport
{
    /** @brief Packages loaded and scanned. */
    int32 PackagesScanned = 0;

    /** @brief Packages where at least one brush was changed. */
    int32 PackagesModified = 0;

    /** @brief Number of replaced brushes. */
    int32 BrushesReplaced = 0;

    /** @brief Number of recompiled blueprints. */
    int32 BlueprintsCompiled = 0;
};

/**
 * @brief Asset creation, lookup and reference replacement helpers.
 */
class FTextureToolkitAssetUtils
{
public:
    /**
     * @brief Converts a folder to a long package path without trailing slash.
     * @param InFolder Folder such as "/Game/UI" or "UI/Atlases" (relative to /Game).
     * @return Normalized folder or an empty string.
     */
    static FString NormalizeFolder(const FString& InFolder);

    /**
     * @brief Returns the package folder of an asset.
     * @param InAsset Asset to inspect.
     * @return Folder such as "/Game/UI".
     */
    static FString GetAssetFolder(const UObject* InAsset);

    /**
     * @brief Removes a leading "T_" from a texture name.
     * @param InTextureName Texture asset name.
     * @return Name without the texture prefix.
     */
    static FString GetBaseName(const FString& InTextureName);

    /**
     * @brief Creates a texture asset or overwrites the source of an existing one (references stay valid).
     * @param InFolder Destination folder.
     * @param InName Asset name.
     * @param InImage Pixels to store.
     * @param InSettings Texture settings to apply.
     * @param OutError Error text on failure.
     * @return The texture or null.
     */
    static UTexture2D* CreateOrUpdateTexture(const FString& InFolder, const FString& InName, const FImage& InImage, const FTextureAssetSettings& InSettings, FText& OutError);

    /**
     * @brief Creates a PaperSprite or updates an existing one to point at a region of a texture.
     * @param InFolder Destination folder.
     * @param InName Asset name.
     * @param InTexture Texture the sprite uses.
     * @param InRect Region in texture pixels.
     * @param OutError Error text on failure.
     * @return The sprite or null.
     */
    static UPaperSprite* CreateOrUpdateSprite(const FString& InFolder, const FString& InName, UTexture2D* InTexture, const FIntRect& InRect, FText& OutError);

    /**
     * @brief Saves the packages of the given assets.
     * @param InAssets Assets to save.
     */
    static void SaveAssets(const TArray<UObject*>& InAssets);

    /**
     * @brief Finds PaperSprites whose source texture is the given texture (registry and loaded objects).
     * @param InTexture Texture to look for.
     * @param OutSprites Found sprites.
     */
    static void FindSpritesUsingTexture(UTexture2D* InTexture, TArray<UPaperSprite*>& OutSprites);

    /**
     * @brief Collects every Texture2D used by an FSlateBrush inside the package of an asset.
     * @param InAsset Asset to scan, usually a Widget Blueprint.
     * @param OutTextures Unique textures.
     */
    static void GatherBrushTextures(UObject* InAsset, TArray<UTexture2D*>& OutTextures);

    /**
     * @brief Replaces FSlateBrush resource objects in packages and recompiles affected blueprints.
     * @param InReplacements Old resource to new resource.
     * @param InPackages Packages to process. Empty processes every hard referencer of the old resources.
     * @param InExcludedPackages Packages that are never touched.
     * @param bInSave Saves modified packages.
     * @param OutReport Statistics.
     */
    static void ReplaceBrushResources(const TMap<UObject*, UObject*>& InReplacements, const TArray<FName>& InPackages, const TSet<FName>& InExcludedPackages, bool bInSave, FTextureReplaceReport& OutReport);

    /**
     * @brief Selects assets in the Content Browser.
     * @param InAssets Assets to show.
     */
    static void BrowseToAssets(const TArray<UObject*>& InAssets);

    /**
     * @brief Opens the asset editor.
     * @param InAsset Asset to open.
     */
    static void OpenEditor(UObject* InAsset);

    /**
     * @brief Shows an editor notification.
     * @param InText Message.
     * @param bInSuccess Success or failure icon.
     */
    static void Notify(const FText& InText, bool bInSuccess);
};
