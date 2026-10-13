// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/EngineTypes.h"
#include "Engine/TextureDefines.h"
#include "Styling/SlateBrush.h"
#include "UObject/SoftObjectPath.h"

#include "TextureToolkitTypes.generated.h"

class UTexture2D;

// ---------------------------------------------------------------------------
// Enums
// ---------------------------------------------------------------------------

/** @brief Working mode of the toolkit window. */
enum class ETextureToolkitMode : uint8
{
    Compress,
    ChannelPack,
    Atlas
};

/** @brief Asset type listed by the toolkit browser. */
enum class ETextureToolkitBrowserAssets : uint8
{
    Textures,
    WidgetBlueprints
};

/** @brief Meaning of a packed channel, used to auto assign textures by name. */
enum class ETextureChannelRole : uint8
{
    None,
    AmbientOcclusion,
    Roughness,
    Metallic,
    Smoothness,
    DetailMask
};

/** @brief Source channel of a texture used by the channel packer. */
UENUM()
enum class ETextureToolkitChannel : uint8
{
    Red,
    Green,
    Blue,
    Alpha,
    Luminance UMETA(DisplayName="Gray (Luminance)")
};

/** @brief Power of two texture dimension. */
UENUM()
enum class ETextureToolkitSize : uint8
{
    Size32 UMETA(DisplayName="32"),
    Size64 UMETA(DisplayName="64"),
    Size128 UMETA(DisplayName="128"),
    Size256 UMETA(DisplayName="256"),
    Size512 UMETA(DisplayName="512"),
    Size1024 UMETA(DisplayName="1024"),
    Size2048 UMETA(DisplayName="2048"),
    Size4096 UMETA(DisplayName="4096"),
    Size8192 UMETA(DisplayName="8192")
};

/** @brief How the Compress mode reduces the texture resolution. */
UENUM()
enum class ETextureCompressMethod : uint8
{
    MaxInGameSize UMETA(DisplayName="Max In-Game Size (non-destructive)", ToolTip="Sets Maximum Texture Size. Source pixels are kept, the built texture is downscaled."),
    LODBias UMETA(DisplayName="LOD Bias (non-destructive)", ToolTip="Sets LOD Bias so the top mips are dropped. Source pixels are kept."),
    ResizeSource UMETA(DisplayName="Resize Source (destructive)", ToolTip="Resamples the source pixels. Also shrinks the asset on disk, cannot be reverted once saved.")
};

/** @brief How the target resolution is computed in Compress mode. */
UENUM()
enum class ETextureResizeRule : uint8
{
    MaxDimension UMETA(DisplayName="Fit Into Max Dimension"),
    ScaleFactor UMETA(DisplayName="Scale Factor")
};

/** @brief Downscale factor used by ETextureResizeRule::ScaleFactor. */
UENUM()
enum class ETextureScaleFactor : uint8
{
    Half UMETA(DisplayName="1/2"),
    Quarter UMETA(DisplayName="1/4"),
    Eighth UMETA(DisplayName="1/8")
};

/** @brief Channel layout presets of the channel packer. */
UENUM()
enum class EChannelPackPreset : uint8
{
    ORM UMETA(DisplayName="ORM: AO, Roughness, Metallic"),
    RMA UMETA(DisplayName="RMA: Roughness, Metallic, AO"),
    MRA UMETA(DisplayName="MRA: Metallic, Roughness, AO"),
    MaskMap UMETA(DisplayName="Mask Map: Metallic, AO, Detail, Smoothness"),
    MetallicSmoothness UMETA(DisplayName="Metallic + Smoothness in Alpha"),
    Custom UMETA(DisplayName="Custom")
};

/** @brief Output resolution rule of the channel packer. */
UENUM()
enum class EChannelPackSizeMode : uint8
{
    LargestInput UMETA(DisplayName="Largest Input"),
    SmallestInput UMETA(DisplayName="Smallest Input"),
    Custom UMETA(DisplayName="Custom")
};

/** @brief Output bit depth of the channel packer. */
UENUM()
enum class EChannelPackBitDepth : uint8
{
    Bit8 UMETA(DisplayName="8 bit (BGRA8)"),
    Bit16 UMETA(DisplayName="16 bit (RGBA16)")
};

/**
 * @brief Converts a size enum to pixels.
 * @param InSize Size enum value.
 * @return Dimension in pixels.
 */
inline int32 TextureToolkitSizeToPixels(ETextureToolkitSize InSize)
{
    return 32 << static_cast<int32>(InSize);
}

/**
 * @brief Converts a scale factor enum to its divider.
 * @param InFactor Scale factor value.
 * @return 2, 4 or 8.
 */
inline int32 TextureScaleFactorToDivider(ETextureScaleFactor InFactor)
{
    return 2 << static_cast<int32>(InFactor);
}

// ---------------------------------------------------------------------------
// Options shown in details views
// ---------------------------------------------------------------------------

/**
 * @brief Settings of the Compress mode.
 */
USTRUCT()
struct FTextureCompressOptions
{
    GENERATED_BODY()

    /** @brief How the resolution reduction is applied. */
    UPROPERTY(EditAnywhere, Category="Resize")
    ETextureCompressMethod Method = ETextureCompressMethod::MaxInGameSize;

    /** @brief How the target resolution is computed. */
    UPROPERTY(EditAnywhere, Category="Resize")
    ETextureResizeRule ResizeRule = ETextureResizeRule::MaxDimension;

    /** @brief Largest allowed side of the result. */
    UPROPERTY(EditAnywhere, meta=(EditCondition="ResizeRule == ETextureResizeRule::MaxDimension", EditConditionHides), Category="Resize")
    ETextureToolkitSize TargetMaxSize = ETextureToolkitSize::Size1024;

    /** @brief Downscale factor applied to the current resolution. */
    UPROPERTY(EditAnywhere, meta=(EditCondition="ResizeRule == ETextureResizeRule::ScaleFactor", EditConditionHides), Category="Resize")
    ETextureScaleFactor ScaleFactor = ETextureScaleFactor::Half;

    /** @brief Textures that already fit the target are not modified at all, format overrides included. */
    UPROPERTY(EditAnywhere, meta=(EditCondition="ResizeRule == ETextureResizeRule::MaxDimension", EditConditionHides), Category="Resize")
    bool bOnlyLargerTextures = true;

    /** @brief Enables CompressionSettings override. */
    UPROPERTY(EditAnywhere, meta=(InlineEditConditionToggle), Category="Format")
    bool bOverrideCompression = false;

    /** @brief Compression settings applied to every texture. */
    UPROPERTY(EditAnywhere, meta=(EditCondition="bOverrideCompression"), Category="Format")
    TEnumAsByte<TextureCompressionSettings> CompressionSettings = TC_Default;

    /** @brief Enables sRGB override. */
    UPROPERTY(EditAnywhere, meta=(InlineEditConditionToggle), Category="Format")
    bool bOverrideSRGB = false;

    /** @brief sRGB flag applied to every texture. */
    UPROPERTY(EditAnywhere, meta=(EditCondition="bOverrideSRGB", DisplayName="sRGB"), Category="Format")
    bool bSRGB = true;

    /** @brief Enables MipGenSettings override. */
    UPROPERTY(EditAnywhere, meta=(InlineEditConditionToggle), Category="Format")
    bool bOverrideMipGen = false;

    /** @brief Mip generation settings applied to every texture. */
    UPROPERTY(EditAnywhere, meta=(EditCondition="bOverrideMipGen"), Category="Format")
    TEnumAsByte<TextureMipGenSettings> MipGenSettings = TMGS_FromTextureGroup;

    /** @brief Enables texture group override. */
    UPROPERTY(EditAnywhere, meta=(InlineEditConditionToggle), Category="Format")
    bool bOverrideLODGroup = false;

    /** @brief Texture group applied to every texture. */
    UPROPERTY(EditAnywhere, meta=(EditCondition="bOverrideLODGroup", DisplayName="Texture Group"), Category="Format")
    TEnumAsByte<TextureGroup> LODGroup = TEXTUREGROUP_World;

    /** @brief Saves modified textures right after Apply. */
    UPROPERTY(EditAnywhere, Category="Apply")
    bool bSaveAfterApply = false;
};

/**
 * @brief Output settings of the Channel Pack mode.
 */
USTRUCT()
struct FTextureChannelPackOptions
{
    GENERATED_BODY()

    /** @brief Content folder of the packed texture. Empty uses the folder of the first input. */
    UPROPERTY(EditAnywhere, meta=(ContentDir), Category="Output")
    FDirectoryPath OutputFolder;

    /** @brief Asset name of the packed texture. Empty generates a name from the first input and the preset. */
    UPROPERTY(EditAnywhere, Category="Output")
    FString OutputName;

    /** @brief Output resolution rule. Inputs are resampled to it. */
    UPROPERTY(EditAnywhere, Category="Output")
    EChannelPackSizeMode SizeMode = EChannelPackSizeMode::LargestInput;

    /** @brief Output width for the Custom size mode. */
    UPROPERTY(EditAnywhere, meta=(EditCondition="SizeMode == EChannelPackSizeMode::Custom", EditConditionHides), Category="Output")
    ETextureToolkitSize CustomWidth = ETextureToolkitSize::Size1024;

    /** @brief Output height for the Custom size mode. */
    UPROPERTY(EditAnywhere, meta=(EditCondition="SizeMode == EChannelPackSizeMode::Custom", EditConditionHides), Category="Output")
    ETextureToolkitSize CustomHeight = ETextureToolkitSize::Size1024;

    /** @brief Bit depth of the stored source data. */
    UPROPERTY(EditAnywhere, Category="Output")
    EChannelPackBitDepth BitDepth = EChannelPackBitDepth::Bit8;

    /** @brief Compression settings of the created texture. */
    UPROPERTY(EditAnywhere, Category="Output")
    TEnumAsByte<TextureCompressionSettings> CompressionSettings = TC_Masks;

    /** @brief sRGB flag of the created texture. Masks should stay linear. */
    UPROPERTY(EditAnywhere, meta=(DisplayName="sRGB"), Category="Output")
    bool bSRGB = false;

    /** @brief Saves the created assets immediately. */
    UPROPERTY(EditAnywhere, Category="Output")
    bool bSaveAssets = true;
};

/**
 * @brief Layout and output settings of the Atlas mode.
 */
USTRUCT()
struct FTextureAtlasOptions
{
    GENERATED_BODY()

    /** @brief Empty pixels reserved around every sprite (per side). Distance between two sprites is twice this value. */
    UPROPERTY(EditAnywhere, meta=(ClampMin="0", ClampMax="64"), Category="Layout")
    int32 Padding = 2;

    /** @brief Fills the padding with the sprite edge pixels to avoid bleeding with filtering and mips. */
    UPROPERTY(EditAnywhere, Category="Layout")
    bool bExtrudeEdges = true;

    /**
     * @brief Cuts fully transparent borders of every texture.
     * @note A trimmed sprite placed into a widget brush stretches to the brush size, so the visible area changes.
     */
    UPROPERTY(EditAnywhere, Category="Layout")
    bool bTrimTransparentBorders = false;

    /** @brief Pixels with alpha at or below this value count as transparent when trimming and when scanning a base atlas. */
    UPROPERTY(EditAnywhere, meta=(ClampMin="0", ClampMax="254"), Category="Layout")
    int32 AlphaThreshold = 0;

    /** @brief Uses a fixed atlas size instead of the smallest power of two that fits. */
    UPROPERTY(EditAnywhere, Category="Layout")
    bool bManualSize = false;

    /** @brief Atlas width when Manual Size is enabled. */
    UPROPERTY(EditAnywhere, meta=(EditCondition="bManualSize"), Category="Layout")
    ETextureToolkitSize ManualWidth = ETextureToolkitSize::Size1024;

    /** @brief Atlas height when Manual Size is enabled. */
    UPROPERTY(EditAnywhere, meta=(EditCondition="bManualSize"), Category="Layout")
    ETextureToolkitSize ManualHeight = ETextureToolkitSize::Size1024;

    /** @brief Upper bound for the automatic atlas size. */
    UPROPERTY(EditAnywhere, meta=(EditCondition="!bManualSize"), Category="Layout")
    ETextureToolkitSize MaxAtlasSize = ETextureToolkitSize::Size4096;

    /** @brief Allows 2:1 atlases when searching for the smallest size. */
    UPROPERTY(EditAnywhere, meta=(EditCondition="!bManualSize"), Category="Layout")
    bool bAllowNonSquare = true;

    /** @brief Content folder of the atlas texture. */
    UPROPERTY(EditAnywhere, meta=(ContentDir), Category="Output")
    FDirectoryPath OutputFolder;

    /** @brief Asset name of the atlas texture. An existing texture with this name is overwritten in place. */
    UPROPERTY(EditAnywhere, Category="Output")
    FString AtlasName = TEXT("T_Atlas");

    /** @brief Compression settings of the atlas texture. BC7 keeps UI quality at 1 byte per pixel, Uncompressed (RGBA8) uses 4. */
    UPROPERTY(EditAnywhere, Category="Output")
    TEnumAsByte<TextureCompressionSettings> CompressionSettings = TC_BC7;

    /** @brief Texture group of the atlas texture. */
    UPROPERTY(EditAnywhere, meta=(DisplayName="Texture Group"), Category="Output")
    TEnumAsByte<TextureGroup> LODGroup = TEXTUREGROUP_UI;

    /** @brief Mip generation of the atlas texture. */
    UPROPERTY(EditAnywhere, Category="Output")
    TEnumAsByte<TextureMipGenSettings> MipGenSettings = TMGS_NoMipmaps;

    /** @brief sRGB flag of the atlas texture. */
    UPROPERTY(EditAnywhere, meta=(DisplayName="sRGB"), Category="Output")
    bool bSRGB = true;

    /** @brief Creates or updates a PaperSprite for every placed texture. */
    UPROPERTY(EditAnywhere, Category="Sprites")
    bool bCreateSprites = true;

    /** @brief Content folder of the sprites. Empty uses <Output Folder>/Sprites. */
    UPROPERTY(EditAnywhere, meta=(ContentDir, EditCondition="bCreateSprites"), Category="Sprites")
    FDirectoryPath SpriteFolder;

    /** @brief Prefix of sprite names. The T_ prefix of the texture name is replaced with it. */
    UPROPERTY(EditAnywhere, meta=(EditCondition="bCreateSprites"), Category="Sprites")
    FString SpritePrefix = TEXT("SPR_");

    /** @brief Saves the atlas, the sprites and the modified widgets immediately. */
    UPROPERTY(EditAnywhere, Category="Output")
    bool bSaveAssets = true;
};

// ---------------------------------------------------------------------------
// Runtime data
// ---------------------------------------------------------------------------

/**
 * @brief One output channel of the channel packer.
 * @note The texture is kept alive by the owning panel (FGCObject).
 */
struct FTextureChannelInput
{
    /** @brief Source texture. Null uses ConstantValue. */
    TObjectPtr<UTexture2D> Texture = nullptr;

    /** @brief Channel of the source texture that is read. */
    ETextureToolkitChannel SourceChannel = ETextureToolkitChannel::Red;

    /** @brief Value written when no texture is assigned, 0..1. */
    float ConstantValue = 0.0f;

    /** @brief Writes 1 - value read from the texture (Roughness to Smoothness and similar). Constants are not inverted. */
    bool bInvert = false;

    /** @brief Display name of the channel in the active preset. */
    FText Label;

    /** @brief Role of the channel in the active preset. */
    ETextureChannelRole Role = ETextureChannelRole::None;
};

/**
 * @brief A texture placed on the atlas canvas.
 * @note The texture is kept alive by the owning panel (FGCObject).
 */
struct FTextureAtlasItem
{
    /** @brief Source texture. */
    TObjectPtr<UTexture2D> Texture = nullptr;

    /** @brief Source resolution of the texture. */
    FIntPoint SourceSize = FIntPoint::ZeroValue;

    /** @brief Bounds of non transparent pixels in source pixels. */
    FIntRect OpaqueBounds;

    /** @brief True when OpaqueBounds was computed. */
    bool bOpaqueBoundsValid = false;

    /** @brief True when the item uses OpaqueBounds instead of the full source. */
    bool bTrimmed = false;

    /** @brief Scale applied to the source rect. */
    float Scale = 1.0f;

    /** @brief Top left corner of the sprite content in atlas pixels. */
    FIntPoint Position = FIntPoint::ZeroValue;

    /** @brief Locked items keep their position on Auto Arrange. */
    bool bLocked = false;

    /** @brief Brush used to draw the item on the canvas and in the list. */
    FSlateBrush Brush;

    /** @brief Returns the used part of the source texture. */
    FIntRect GetSourceRect() const;

    /** @brief Returns the size of the sprite in the atlas. */
    FIntPoint GetSize() const;

    /** @brief Returns the content rect of the sprite in the atlas. */
    FIntRect GetRect() const;

    /** @brief Updates the brush resource and UV region. */
    void RefreshBrush();
};
