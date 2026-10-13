// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "ImageCore.h"

#include "Core/TextureToolkitTypes.h"

class UTexture2D;

/**
 * @brief Cache of input images resampled to a fixed size, keyed by texture.
 */
using FTextureChannelImageCache = TMap<TWeakObjectPtr<UTexture2D>, FImage>;

/**
 * @brief Channel packing and splitting.
 * @note Values are transferred as stored (no gamma conversion), which is what mask maps expect.
 */
class FTextureChannelPacker
{
public:
    /**
     * @brief Fills labels, constants and invert flags of the four outputs for a preset.
     * @param InPreset Preset to apply.
     * @param InOutInputs Four channel inputs. Textures are kept.
     */
    static void ApplyPreset(EChannelPackPreset InPreset, TArray<FTextureChannelInput>& InOutInputs);

    /**
     * @brief Returns the suffix used for generated names (ORM, RMA, ...).
     * @param InPreset Preset.
     * @return Suffix without underscore.
     */
    static FString GetPresetSuffix(EChannelPackPreset InPreset);

    /**
     * @brief Guesses the role of a texture from its name tokens (_AO, _Roughness, _Metallic, _Gloss, ...).
     * @param InTextureName Texture asset name.
     * @return Guessed role or None.
     */
    static ETextureChannelRole GuessRole(const FString& InTextureName);

    /**
     * @brief Assigns textures to the inputs: by role guessed from the name first, then to empty slots.
     * @param InTextures Textures to assign.
     * @param InOutInputs Four channel inputs.
     */
    static void AutoAssign(const TArray<UTexture2D*>& InTextures, TArray<FTextureChannelInput>& InOutInputs);

    /**
     * @brief Resolves the folder and asset name of the packed texture.
     * @param InInputs Four channel inputs.
     * @param InPreset Active preset.
     * @param InOptions Output options.
     * @param OutFolder Destination folder.
     * @param OutName Asset name.
     * @return False when no input texture is assigned.
     */
    static bool ResolveOutput(const TArray<FTextureChannelInput>& InInputs, EChannelPackPreset InPreset, const FTextureChannelPackOptions& InOptions, FString& OutFolder, FString& OutName);

    /**
     * @brief Computes the output resolution for the given inputs and options.
     * @param InInputs Four channel inputs.
     * @param InOptions Output options.
     * @return Output size, at least 1x1.
     */
    static FIntPoint ComputeOutputSize(const TArray<FTextureChannelInput>& InInputs, const FTextureChannelPackOptions& InOptions);

    /**
     * @brief Packs the inputs into an RGBA32F image.
     * @param InInputs Four channel inputs (R, G, B, A).
     * @param InSize Output resolution. Inputs are resampled to it.
     * @param InOutCache Optional cache of resampled inputs, reused when the size matches.
     * @param OutImage RGBA32F linear image.
     * @return False when an input could not be read.
     */
    static bool Pack(const TArray<FTextureChannelInput>& InInputs, const FIntPoint& InSize, FTextureChannelImageCache* InOutCache, FImage& OutImage);

    /**
     * @brief Packs the inputs and writes a texture asset.
     * @param InInputs Four channel inputs.
     * @param InPreset Active preset, used for the generated name.
     * @param InOptions Output options.
     * @param OutError Error text on failure.
     * @return Created or updated texture.
     */
    static UTexture2D* CreatePackedTexture(const TArray<FTextureChannelInput>& InInputs, EChannelPackPreset InPreset, const FTextureChannelPackOptions& InOptions, FText& OutError);

    /**
     * @brief Writes selected channels of a texture into separate grayscale textures named <Texture>_R / _G / _B / _A.
     * @param InTexture Texture to split.
     * @param InChannelMask Channels to export: bit 0 = R, bit 1 = G, bit 2 = B, bit 3 = A.
     * @param InFolder Destination folder. Empty uses the folder of the texture.
     * @param bInSave Saves the created textures.
     * @param OutTextures Created or updated textures.
     * @param OutError Error text on failure.
     * @return True when every requested channel was written.
     */
    static bool SplitChannels(UTexture2D* InTexture, uint8 InChannelMask, const FString& InFolder, bool bInSave, TArray<UTexture2D*>& OutTextures, FText& OutError);

protected:
    /**
     * @brief Returns the first assigned texture.
     * @param InInputs Four channel inputs.
     * @return First texture or null.
     */
    static UTexture2D* GetFirstTexture(const TArray<FTextureChannelInput>& InInputs);
};
