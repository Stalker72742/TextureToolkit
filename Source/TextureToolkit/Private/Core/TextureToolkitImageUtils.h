// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/TextureDefines.h"
#include "ImageCore.h"
#include "PixelFormat.h"

#include "Core/TextureToolkitTypes.h"

class UTexture2D;

/**
 * @brief Pixel level helpers shared by all toolkit modes.
 * @note Every function works on mip 0 of the texture source data (editor only).
 */
class FTextureToolkitImageUtils
{
public:
    /**
     * @brief Checks that the texture source can be read by the toolkit.
     * @param InTexture Texture to check.
     * @param OutReason Optional reason when the texture is rejected.
     * @return True for single block, single layer textures with valid source data.
     */
    static bool CanReadSource(const UTexture2D* InTexture, FText* OutReason = nullptr);

    /**
     * @brief Reads mip 0 of the source in its stored format and gamma space.
     * @param InTexture Texture to read.
     * @param OutImage Source pixels.
     * @return True on success.
     */
    static bool ReadSourceImage(UTexture2D* InTexture, FImage& OutImage);

    /**
     * @brief Reads mip 0 as RGBA32F without any gamma conversion (stored values as they are).
     * @note Used for masks: an 8 bit value of 128 becomes 0.502 regardless of the sRGB flag.
     * @param InTexture Texture to read.
     * @param OutImage RGBA32F linear image.
     * @return True on success.
     */
    static bool ReadRawImage(UTexture2D* InTexture, FImage& OutImage);

    /**
     * @brief Reads mip 0 as BGRA8 in sRGB space, converting from the texture gamma when needed.
     * @param InTexture Texture to read.
     * @param OutImage BGRA8 sRGB image.
     * @return True on success.
     */
    static bool ReadDisplayImage(UTexture2D* InTexture, FImage& OutImage);

    /**
     * @brief Computes the bounds of pixels with alpha above the threshold.
     * @param InImage BGRA8 image.
     * @param InAlphaThreshold Alpha values at or below this are transparent.
     * @return Opaque bounds, or the full image when every pixel is transparent.
     */
    static FIntRect ComputeOpaqueBounds(const FImage& InImage, uint8 InAlphaThreshold);

    /**
     * @brief Copies a rectangle into a new image of the same format.
     * @param InSource Source image.
     * @param InRect Rectangle to copy, clipped to the source.
     * @param OutImage Result.
     */
    static void CropImage(const FImage& InSource, const FIntRect& InRect, FImage& OutImage);

    /**
     * @brief Copies an image into another one of the same format, clipped to the destination.
     * @param InSource Image to copy.
     * @param InOutDest Destination image.
     * @param InDestPosition Top left corner in the destination.
     */
    static void BlitImage(const FImage& InSource, FImage& InOutDest, const FIntPoint& InDestPosition);

    /**
     * @brief Repeats the border pixels of a rectangle outwards.
     * @param InOutImage Image to modify.
     * @param InRect Rectangle whose edges are extruded.
     * @param InAmount Number of pixels written on every side.
     */
    static void ExtrudeEdges(FImage& InOutImage, const FIntRect& InRect, int32 InAmount);

    /**
     * @brief Reads one channel of a color.
     * @param InColor Color to read.
     * @param InChannel Channel to read.
     * @return Channel value.
     */
    static float GetChannelValue(const FLinearColor& InColor, ETextureToolkitChannel InChannel);

    /**
     * @brief Estimates GPU memory of a 2D texture.
     * @param InSize Top mip resolution.
     * @param InFormat Pixel format.
     * @param bInWithMips True to include the full mip chain.
     * @return Size in bytes.
     */
    static int64 EstimateMemoryBytes(const FIntPoint& InSize, EPixelFormat InFormat, bool bInWithMips);

    /**
     * @brief Guesses the pixel format produced by compression settings on desktop platforms.
     * @param InSettings Compression settings.
     * @param bInHasAlpha True when the texture has a meaningful alpha channel.
     * @return Expected pixel format.
     */
    static EPixelFormat GuessPixelFormat(TextureCompressionSettings InSettings, bool bInHasAlpha);

    /**
     * @brief Formats a byte count as B / KB / MB.
     * @param InBytes Byte count.
     * @return Human readable text.
     */
    static FString FormatBytes(int64 InBytes);

    /**
     * @brief Halves a size until its largest side fits (mip style downscale).
     * @param InSize Size to reduce.
     * @param InMaxDimension Largest allowed side, 0 or less means unlimited.
     * @param OutSteps Optional number of halvings.
     * @return Reduced size.
     */
    static FIntPoint HalveToFit(const FIntPoint& InSize, int32 InMaxDimension, int32* OutSteps = nullptr);

    /**
     * @brief Proportionally scales a size so its largest side fits. Power of two sizes are halved instead.
     * @param InSize Size to reduce.
     * @param InMaxDimension Largest allowed side.
     * @return Reduced size, never larger than the input.
     */
    static FIntPoint ScaleToFit(const FIntPoint& InSize, int32 InMaxDimension);
};
