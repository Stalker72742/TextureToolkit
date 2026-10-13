// Fill out your copyright notice in the Description page of Project Settings.

#include "Core/TextureToolkitTypes.h"

#include "Engine/Texture2D.h"

FIntRect FTextureAtlasItem::GetSourceRect() const
{
    if (bTrimmed && bOpaqueBoundsValid)
    {
        return OpaqueBounds;
    }
    return FIntRect(FIntPoint::ZeroValue, SourceSize);
}

FIntPoint FTextureAtlasItem::GetSize() const
{
    const FIntPoint sourceSize = GetSourceRect().Size();
    return FIntPoint(
        FMath::Max(1, FMath::RoundToInt(sourceSize.X * Scale)),
        FMath::Max(1, FMath::RoundToInt(sourceSize.Y * Scale)));
}

FIntRect FTextureAtlasItem::GetRect() const
{
    return FIntRect(Position, Position + GetSize());
}

void FTextureAtlasItem::RefreshBrush()
{
    Brush.DrawAs = ESlateBrushDrawType::Image;
    Brush.Tiling = ESlateBrushTileType::NoTile;
    Brush.SetResourceObject(Texture);

    const FIntRect sourceRect = GetSourceRect();
    Brush.ImageSize = FVector2D(FMath::Max(1, sourceRect.Width()), FMath::Max(1, sourceRect.Height()));
    if (SourceSize.X > 0 && SourceSize.Y > 0)
    {
        const FVector2f invSize(1.0f / SourceSize.X, 1.0f / SourceSize.Y);
        Brush.SetUVRegion(FBox2f(
            FVector2f(sourceRect.Min.X * invSize.X, sourceRect.Min.Y * invSize.Y),
            FVector2f(sourceRect.Max.X * invSize.X, sourceRect.Max.Y * invSize.Y)));
    }
}
