// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "PixelFormat.h"

#include "Core/TextureToolkitTypes.h"

class UTexture2D;

/**
 * @brief Before / after state of one texture in Compress mode.
 */
struct FTextureCompressPlan
{
    /** @brief Resolution of the source pixels. */
    FIntPoint SourceSize = FIntPoint::ZeroValue;

    /** @brief Current resolution of the top resident mip in game. */
    FIntPoint CurrentSize = FIntPoint::ZeroValue;

    /** @brief Current pixel format. */
    EPixelFormat CurrentFormat = PF_Unknown;

    /** @brief Estimated current memory. */
    int64 CurrentBytes = 0;

    /** @brief Source resolution after Apply. */
    FIntPoint NewSourceSize = FIntPoint::ZeroValue;

    /** @brief In game resolution after Apply. */
    FIntPoint NewSize = FIntPoint::ZeroValue;

    /** @brief Expected pixel format after Apply. */
    EPixelFormat NewFormat = PF_Unknown;

    /** @brief Estimated memory after Apply. */
    int64 NewBytes = 0;

    /** @brief MaxTextureSize written on Apply. */
    int32 NewMaxTextureSize = 0;

    /** @brief LODBias written on Apply. */
    int32 NewLODBias = 0;

    /** @brief True when Apply changes the texture. */
    bool bChanged = false;

    /** @brief True when the texture cannot be processed. */
    bool bInvalid = false;

    /** @brief Short explanation shown in the list. */
    FText Note;
};

/**
 * @brief Computes and applies Compress mode changes.
 */
class FTextureCompressor
{
public:
    /**
     * @brief Computes what Apply would do with a texture.
     * @param InTexture Texture to inspect.
     * @param InOptions Compress settings.
     * @return Plan with current and expected values.
     */
    static FTextureCompressPlan BuildPlan(UTexture2D* InTexture, const FTextureCompressOptions& InOptions);

    /**
     * @brief Applies a plan to a texture (inside the caller's transaction).
     * @param InTexture Texture to modify.
     * @param InPlan Plan built with the same options.
     * @param InOptions Compress settings.
     * @param OutError Error text on failure.
     * @return True when the texture was modified.
     */
    static bool ApplyPlan(UTexture2D* InTexture, const FTextureCompressPlan& InPlan, const FTextureCompressOptions& InOptions, FText& OutError);

protected:
    /**
     * @brief Returns the resolution of the built top mip before LOD bias.
     * @param InTexture Texture to inspect.
     * @return Platform data size, or the source size reduced by MaxTextureSize when not built yet.
     */
    static FIntPoint GetBuiltSize(UTexture2D* InTexture);
};
