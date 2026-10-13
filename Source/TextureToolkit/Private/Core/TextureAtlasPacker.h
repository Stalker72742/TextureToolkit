// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"

/**
 * @brief Input of the atlas packer.
 */
struct FTextureAtlasPackParams
{
    /** @brief Empty pixels reserved around every rectangle (per side). */
    int32 Padding = 2;

    /** @brief Regions that must stay free of new rectangles (locked sprites, base atlas content). */
    TArray<FIntRect> Obstacles;

    /** @brief Fixed atlas size. When zero the smallest fitting power of two size is searched. */
    FIntPoint FixedSize = FIntPoint::ZeroValue;

    /** @brief Smallest atlas size allowed by the automatic search (base atlas size). */
    FIntPoint MinSize = FIntPoint::ZeroValue;

    /** @brief Largest side allowed by the automatic search. */
    int32 MaxSize = 4096;

    /** @brief Allows 2:1 sizes in the automatic search. */
    bool bAllowNonSquare = true;
};

/**
 * @brief Rectangle packer based on a cell occupancy grid and a summed area table.
 * @note Works with arbitrary obstacles, which is required when new sprites are added to an existing atlas.
 *       Rectangles are sorted by height and placed at the top-most, then left-most free position.
 */
class FTextureAtlasPacker
{
public:
    /**
     * @brief Packs rectangles.
     * @param InParams Packing parameters.
     * @param InSizes Content size of every rectangle.
     * @param OutAtlasSize Resulting atlas size.
     * @param OutPositions Top left corner of the content of every rectangle.
     * @return False when the rectangles do not fit into the allowed sizes.
     */
    static bool Pack(const FTextureAtlasPackParams& InParams, const TArray<FIntPoint>& InSizes, FIntPoint& OutAtlasSize, TArray<FIntPoint>& OutPositions);

protected:
    /**
     * @brief Packs rectangles into a fixed size.
     * @param InAtlasSize Atlas size.
     * @param InParams Packing parameters.
     * @param InSizes Content sizes.
     * @param OutPositions Content positions.
     * @return False when a rectangle does not fit.
     */
    static bool PackFixed(const FIntPoint& InAtlasSize, const FTextureAtlasPackParams& InParams, const TArray<FIntPoint>& InSizes, TArray<FIntPoint>& OutPositions);

    /**
     * @brief Builds the list of candidate sizes for the automatic search, smallest first.
     * @param InParams Packing parameters.
     * @return Candidate sizes.
     */
    static TArray<FIntPoint> BuildCandidateSizes(const FTextureAtlasPackParams& InParams);
};
