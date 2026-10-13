// Fill out your copyright notice in the Description page of Project Settings.

#include "Core/TextureAtlasPacker.h"

namespace TextureAtlasPackerPrivate
{
    constexpr int32 MaxGridCells = 1024;
}

bool FTextureAtlasPacker::Pack(const FTextureAtlasPackParams& InParams, const TArray<FIntPoint>& InSizes, FIntPoint& OutAtlasSize, TArray<FIntPoint>& OutPositions)
{
    OutPositions.Reset();

    if (InParams.FixedSize.X > 0 && InParams.FixedSize.Y > 0)
    {
        OutAtlasSize = InParams.FixedSize;
        return PackFixed(InParams.FixedSize, InParams, InSizes, OutPositions);
    }

    if (InSizes.Num() == 0)
    {
        OutAtlasSize = (InParams.MinSize.X > 0 && InParams.MinSize.Y > 0) ? InParams.MinSize : FIntPoint(256, 256);
        return true;
    }

    const int32 padding = FMath::Max(0, InParams.Padding);
    int64 requiredArea = 0;
    FIntPoint largestBlock = FIntPoint::ZeroValue;
    for (const FIntPoint& size : InSizes)
    {
        const FIntPoint block(size.X + padding * 2, size.Y + padding * 2);
        requiredArea += static_cast<int64>(block.X) * block.Y;
        largestBlock = largestBlock.ComponentMax(block);
    }
    for (const FIntRect& obstacle : InParams.Obstacles)
    {
        requiredArea += static_cast<int64>(obstacle.Width() + padding * 2) * (obstacle.Height() + padding * 2);
    }

    const TArray<FIntPoint> candidates = BuildCandidateSizes(InParams);
    for (const FIntPoint& candidate : candidates)
    {
        if (static_cast<int64>(candidate.X) * candidate.Y < requiredArea || largestBlock.X > candidate.X || largestBlock.Y > candidate.Y)
        {
            continue;
        }

        TArray<FIntPoint> positions;
        if (PackFixed(candidate, InParams, InSizes, positions))
        {
            OutAtlasSize = candidate;
            OutPositions = MoveTemp(positions);
            return true;
        }
    }
    return false;
}

bool FTextureAtlasPacker::PackFixed(const FIntPoint& InAtlasSize, const FTextureAtlasPackParams& InParams, const TArray<FIntPoint>& InSizes, TArray<FIntPoint>& OutPositions)
{
    const int32 padding = FMath::Max(0, InParams.Padding);
    const int32 cellSize = FMath::Max(1, FMath::DivideAndRoundUp(FMath::Max(InAtlasSize.X, InAtlasSize.Y), TextureAtlasPackerPrivate::MaxGridCells));
    const int32 gridWidth = InAtlasSize.X / cellSize;
    const int32 gridHeight = InAtlasSize.Y / cellSize;
    if (gridWidth <= 0 || gridHeight <= 0)
    {
        return false;
    }

    TArray<uint8> occupied;
    occupied.SetNumZeroed(gridWidth * gridHeight);

    auto markCells = [&occupied, gridWidth, gridHeight](int32 InX0, int32 InY0, int32 InX1, int32 InY1)
    {
        const int32 x0 = FMath::Clamp(InX0, 0, gridWidth);
        const int32 y0 = FMath::Clamp(InY0, 0, gridHeight);
        const int32 x1 = FMath::Clamp(InX1, 0, gridWidth);
        const int32 y1 = FMath::Clamp(InY1, 0, gridHeight);
        for (int32 y = y0; y < y1; ++y)
        {
            FMemory::Memset(occupied.GetData() + y * gridWidth + x0, 1, FMath::Max(0, x1 - x0));
        }
    };

    for (const FIntRect& obstacle : InParams.Obstacles)
    {
        const FIntRect padded(obstacle.Min - FIntPoint(padding), obstacle.Max + FIntPoint(padding));
        markCells(
            FMath::Max(0, padded.Min.X) / cellSize,
            FMath::Max(0, padded.Min.Y) / cellSize,
            FMath::DivideAndRoundUp(FMath::Max(0, padded.Max.X), cellSize),
            FMath::DivideAndRoundUp(FMath::Max(0, padded.Max.Y), cellSize));
    }

    TArray<int32> order;
    order.Reserve(InSizes.Num());
    for (int32 index = 0; index < InSizes.Num(); ++index)
    {
        order.Add(index);
    }
    order.Sort([&InSizes](int32 InA, int32 InB)
    {
        if (InSizes[InA].Y != InSizes[InB].Y)
        {
            return InSizes[InA].Y > InSizes[InB].Y;
        }
        if (InSizes[InA].X != InSizes[InB].X)
        {
            return InSizes[InA].X > InSizes[InB].X;
        }
        return InA < InB;
    });

    const int32 satStride = gridWidth + 1;
    TArray<int32> sat;
    sat.SetNumZeroed(satStride * (gridHeight + 1));

    OutPositions.SetNum(InSizes.Num());
    for (const int32 index : order)
    {
        const int32 blockWidth = FMath::DivideAndRoundUp(InSizes[index].X + padding * 2, cellSize);
        const int32 blockHeight = FMath::DivideAndRoundUp(InSizes[index].Y + padding * 2, cellSize);
        if (blockWidth > gridWidth || blockHeight > gridHeight)
        {
            return false;
        }

        for (int32 y = 1; y <= gridHeight; ++y)
        {
            int32 rowSum = 0;
            const uint8* occupiedRow = occupied.GetData() + (y - 1) * gridWidth;
            int32* satRow = sat.GetData() + y * satStride;
            const int32* satPrevRow = sat.GetData() + (y - 1) * satStride;
            for (int32 x = 1; x <= gridWidth; ++x)
            {
                rowSum += occupiedRow[x - 1];
                satRow[x] = satPrevRow[x] + rowSum;
            }
        }

        int32 foundX = INDEX_NONE;
        int32 foundY = INDEX_NONE;
        for (int32 y = 0; y + blockHeight <= gridHeight && foundX == INDEX_NONE; ++y)
        {
            const int32* satTop = sat.GetData() + y * satStride;
            const int32* satBottom = sat.GetData() + (y + blockHeight) * satStride;
            for (int32 x = 0; x + blockWidth <= gridWidth; ++x)
            {
                const int32 sum = satBottom[x + blockWidth] - satTop[x + blockWidth] - satBottom[x] + satTop[x];
                if (sum == 0)
                {
                    foundX = x;
                    foundY = y;
                    break;
                }
            }
        }

        if (foundX == INDEX_NONE)
        {
            return false;
        }

        markCells(foundX, foundY, foundX + blockWidth, foundY + blockHeight);
        OutPositions[index] = FIntPoint(foundX * cellSize + padding, foundY * cellSize + padding);
    }
    return true;
}

TArray<FIntPoint> FTextureAtlasPacker::BuildCandidateSizes(const FTextureAtlasPackParams& InParams)
{
    TArray<FIntPoint> candidates;
    const int32 maxSize = FMath::Max(16, InParams.MaxSize);
    for (int32 width = 16; width <= maxSize; width *= 2)
    {
        for (int32 height = 16; height <= maxSize; height *= 2)
        {
            if (!InParams.bAllowNonSquare && width != height)
            {
                continue;
            }
            if (width > height * 2 || height > width * 2)
            {
                continue;
            }
            candidates.Add(FIntPoint(width, height));
        }
    }

    if (InParams.MinSize.X > 0 && InParams.MinSize.Y > 0)
    {
        TArray<FIntPoint> clamped;
        clamped.Add(InParams.MinSize);
        for (const FIntPoint& candidate : candidates)
        {
            clamped.AddUnique(candidate.ComponentMax(InParams.MinSize));
        }
        candidates = MoveTemp(clamped);
    }

    candidates.Sort([](const FIntPoint& InA, const FIntPoint& InB)
    {
        const int64 areaA = static_cast<int64>(InA.X) * InA.Y;
        const int64 areaB = static_cast<int64>(InB.X) * InB.Y;
        if (areaA != areaB)
        {
            return areaA < areaB;
        }
        const int32 squarenessA = FMath::Abs(InA.X - InA.Y);
        const int32 squarenessB = FMath::Abs(InB.X - InB.Y);
        if (squarenessA != squarenessB)
        {
            return squarenessA < squarenessB;
        }
        return InA.X > InB.X;
    });
    return candidates;
}
