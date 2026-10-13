// Fill out your copyright notice in the Description page of Project Settings.

#include "Widgets/STextureAtlasCanvas.h"

#include "Framework/Application/MenuStack.h"
#include "Framework/Application/SlateApplication.h"
#include "Rendering/DrawElements.h"
#include "Styling/AppStyle.h"

namespace TextureAtlasCanvasPrivate
{
    const FLinearColor BackgroundColor(0.008f, 0.008f, 0.008f);
    const FLinearColor AtlasColor(0.035f, 0.035f, 0.04f);
    const FLinearColor GridColor(1.0f, 1.0f, 1.0f, 0.06f);
    const FLinearColor BorderColor(0.75f, 0.75f, 0.75f, 1.0f);
    const FLinearColor OccupiedColor(0.9f, 0.25f, 0.2f, 0.16f);
    const FLinearColor ItemColor(1.0f, 1.0f, 1.0f, 0.3f);
    const FLinearColor SelectedColor(0.2f, 0.7f, 1.0f, 1.0f);
    const FLinearColor InvalidColor(1.0f, 0.2f, 0.15f, 1.0f);
    const FLinearColor LockedColor(1.0f, 0.8f, 0.2f, 0.9f);

    constexpr float MinZoom = 0.02f;
    constexpr float MaxZoom = 32.0f;
    constexpr float ZoomStep = 1.15f;
    constexpr int32 ShiftSnap = 16;
}

void STextureAtlasCanvas::Construct(const FArguments& InArgs)
{
    Items = InArgs._Items;
    BaseOccupied = InArgs._BaseOccupied;
    AtlasSize = InArgs._AtlasSize;
    Padding = InArgs._Padding;
    BaseAtlasBrush = InArgs._BaseAtlasBrush;
    BaseAtlasSize = InArgs._BaseAtlasSize;
    OnItemSelected = InArgs._OnItemSelected;
    OnItemsMoved = InArgs._OnItemsMoved;
    OnItemContextMenu = InArgs._OnItemContextMenu;
    OnDeleteRequested = InArgs._OnDeleteRequested;
}

FVector2f STextureAtlasCanvas::AtlasToLocal(const FVector2f& InAtlasPosition) const
{
    return InAtlasPosition * Zoom + ViewOffset;
}

FVector2f STextureAtlasCanvas::LocalToAtlas(const FVector2f& InLocalPosition) const
{
    return (InLocalPosition - ViewOffset) / Zoom;
}

TSharedPtr<FTextureAtlasItem> STextureAtlasCanvas::HitTest(const FVector2f& InAtlasPosition) const
{
    if (!Items)
    {
        return nullptr;
    }

    for (int32 index = Items->Num() - 1; index >= 0; --index)
    {
        const TSharedPtr<FTextureAtlasItem>& item = (*Items)[index];
        if (!item.IsValid())
        {
            continue;
        }

        const FIntRect rect = item->GetRect();
        if (InAtlasPosition.X >= rect.Min.X && InAtlasPosition.X < rect.Max.X && InAtlasPosition.Y >= rect.Min.Y && InAtlasPosition.Y < rect.Max.Y)
        {
            return item;
        }
    }
    return nullptr;
}

FIntPoint STextureAtlasCanvas::ClampPosition(const FTextureAtlasItem& InItem, const FIntPoint& InPosition) const
{
    const FIntPoint atlasSize = AtlasSize.Get();
    const FIntPoint itemSize = InItem.GetSize();
    return FIntPoint(
        FMath::Clamp(InPosition.X, 0, FMath::Max(0, atlasSize.X - itemSize.X)),
        FMath::Clamp(InPosition.Y, 0, FMath::Max(0, atlasSize.Y - itemSize.Y)));
}

void STextureAtlasCanvas::DrawOutline(FSlateWindowElementList& OutDrawElements, int32 InLayerId, const FGeometry& InGeometry, const FVector2f& InMin, const FVector2f& InMax, const FLinearColor& InColor, float InThickness) const
{
    TArray<FVector2f> points;
    points.Add(InMin);
    points.Add(FVector2f(InMax.X, InMin.Y));
    points.Add(InMax);
    points.Add(FVector2f(InMin.X, InMax.Y));
    points.Add(InMin);
    FSlateDrawElement::MakeLines(OutDrawElements, InLayerId, InGeometry.ToPaintGeometry(), MoveTemp(points), ESlateDrawEffect::None, InColor, true, InThickness);
}

void STextureAtlasCanvas::FindInvalidItems(const TArray<TSharedPtr<FTextureAtlasItem>>& InItems, const TArray<FIntRect>& InOccupied, const FIntPoint& InAtlasSize, int32 InPadding, TSet<const FTextureAtlasItem*>& OutInvalid)
{
    const FIntPoint padding(FMath::Max(0, InPadding));
    const FIntRect atlasRect(FIntPoint::ZeroValue, InAtlasSize);

    for (int32 indexA = 0; indexA < InItems.Num(); ++indexA)
    {
        const TSharedPtr<FTextureAtlasItem>& itemA = InItems[indexA];
        if (!itemA.IsValid())
        {
            continue;
        }

        const FIntRect rectA = itemA->GetRect();
        if (!atlasRect.Contains(rectA))
        {
            OutInvalid.Add(itemA.Get());
        }

        const FIntRect paddedA(rectA.Min - padding, rectA.Max + padding);
        for (const FIntRect& occupied : InOccupied)
        {
            if (paddedA.Intersect(FIntRect(occupied.Min - padding, occupied.Max + padding)))
            {
                OutInvalid.Add(itemA.Get());
                break;
            }
        }

        for (int32 indexB = indexA + 1; indexB < InItems.Num(); ++indexB)
        {
            const TSharedPtr<FTextureAtlasItem>& itemB = InItems[indexB];
            if (!itemB.IsValid())
            {
                continue;
            }

            const FIntRect rectB = itemB->GetRect();
            if (paddedA.Intersect(FIntRect(rectB.Min - padding, rectB.Max + padding)))
            {
                OutInvalid.Add(itemA.Get());
                OutInvalid.Add(itemB.Get());
            }
        }
    }
}

int32 STextureAtlasCanvas::OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
    using namespace TextureAtlasCanvasPrivate;

    const FSlateBrush* whiteBrush = FAppStyle::GetBrush("WhiteBrush");
    const FIntPoint atlasSize = AtlasSize.Get();
    const FVector2f atlasMin = AtlasToLocal(FVector2f::ZeroVector);
    const FVector2f atlasMax = AtlasToLocal(FVector2f(atlasSize.X, atlasSize.Y));

    FSlateDrawElement::MakeBox(OutDrawElements, LayerId, AllottedGeometry.ToPaintGeometry(), whiteBrush, ESlateDrawEffect::None, BackgroundColor);

    FSlateDrawElement::MakeBox(OutDrawElements, LayerId + 1,
        AllottedGeometry.ToPaintGeometry(atlasMax - atlasMin, FSlateLayoutTransform(atlasMin)),
        whiteBrush, ESlateDrawEffect::None, AtlasColor);

    if (atlasSize.X > 0 && atlasSize.Y > 0)
    {
        int32 gridStep = 64;
        while (gridStep * Zoom < 24.0f && gridStep < 4096)
        {
            gridStep *= 2;
        }
        while (gridStep > 8 && gridStep * Zoom > 160.0f)
        {
            gridStep /= 2;
        }

        for (int32 x = gridStep; x < atlasSize.X; x += gridStep)
        {
            TArray<FVector2f> points;
            points.Add(AtlasToLocal(FVector2f(x, 0.0f)));
            points.Add(AtlasToLocal(FVector2f(x, atlasSize.Y)));
            FSlateDrawElement::MakeLines(OutDrawElements, LayerId + 2, AllottedGeometry.ToPaintGeometry(), MoveTemp(points), ESlateDrawEffect::None, GridColor, false, 1.0f);
        }
        for (int32 y = gridStep; y < atlasSize.Y; y += gridStep)
        {
            TArray<FVector2f> points;
            points.Add(AtlasToLocal(FVector2f(0.0f, y)));
            points.Add(AtlasToLocal(FVector2f(atlasSize.X, y)));
            FSlateDrawElement::MakeLines(OutDrawElements, LayerId + 2, AllottedGeometry.ToPaintGeometry(), MoveTemp(points), ESlateDrawEffect::None, GridColor, false, 1.0f);
        }
    }

    if (const FSlateBrush* baseBrush = BaseAtlasBrush.Get())
    {
        const FIntPoint baseSize = BaseAtlasSize.Get();
        if (baseSize.X > 0 && baseSize.Y > 0)
        {
            FSlateDrawElement::MakeBox(OutDrawElements, LayerId + 3,
                AllottedGeometry.ToPaintGeometry(FVector2f(baseSize.X, baseSize.Y) * Zoom, FSlateLayoutTransform(atlasMin)),
                baseBrush, ESlateDrawEffect::None, FLinearColor::White);
        }
    }

    if (BaseOccupied)
    {
        for (const FIntRect& occupied : *BaseOccupied)
        {
            const FVector2f rectMin = AtlasToLocal(FVector2f(occupied.Min.X, occupied.Min.Y));
            const FVector2f rectMax = AtlasToLocal(FVector2f(occupied.Max.X, occupied.Max.Y));
            FSlateDrawElement::MakeBox(OutDrawElements, LayerId + 4,
                AllottedGeometry.ToPaintGeometry(rectMax - rectMin, FSlateLayoutTransform(rectMin)),
                whiteBrush, ESlateDrawEffect::None, OccupiedColor);
        }
    }

    if (Items)
    {
        TSet<const FTextureAtlasItem*> invalidItems;
        static const TArray<FIntRect> emptyOccupied;
        FindInvalidItems(*Items, BaseOccupied ? *BaseOccupied : emptyOccupied, atlasSize, Padding.Get(), invalidItems);

        const TSharedPtr<FTextureAtlasItem> selectedItem = SelectedItem.Pin();
        for (const TSharedPtr<FTextureAtlasItem>& item : *Items)
        {
            if (!item.IsValid())
            {
                continue;
            }

            const FIntRect rect = item->GetRect();
            const FVector2f rectMin = AtlasToLocal(FVector2f(rect.Min.X, rect.Min.Y));
            const FVector2f rectMax = AtlasToLocal(FVector2f(rect.Max.X, rect.Max.Y));

            FSlateDrawElement::MakeBox(OutDrawElements, LayerId + 5,
                AllottedGeometry.ToPaintGeometry(rectMax - rectMin, FSlateLayoutTransform(rectMin)),
                &item->Brush, ESlateDrawEffect::None, FLinearColor::White);

            FLinearColor outlineColor = ItemColor;
            float thickness = 1.0f;
            if (invalidItems.Contains(item.Get()))
            {
                outlineColor = InvalidColor;
                thickness = 1.5f;
            }
            else if (item->bLocked)
            {
                outlineColor = LockedColor;
            }
            if (item == selectedItem)
            {
                outlineColor = invalidItems.Contains(item.Get()) ? InvalidColor : SelectedColor;
                thickness = 2.0f;
            }
            DrawOutline(OutDrawElements, LayerId + 6, AllottedGeometry, rectMin, rectMax, outlineColor, thickness);
        }
    }

    DrawOutline(OutDrawElements, LayerId + 7, AllottedGeometry, atlasMin, atlasMax, BorderColor, 1.5f);
    return LayerId + 8;
}

FVector2D STextureAtlasCanvas::ComputeDesiredSize(float LayoutScaleMultiplier) const
{
    return FVector2D(320.0f, 320.0f);
}

void STextureAtlasCanvas::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
    const FIntPoint atlasSize = AtlasSize.Get();
    if (atlasSize != LastFitAtlasSize)
    {
        bFitRequested = true;
    }

    const FVector2f localSize = AllottedGeometry.GetLocalSize();
    if (bFitRequested && localSize.X > 1.0f && localSize.Y > 1.0f && atlasSize.X > 0 && atlasSize.Y > 0)
    {
        const float margin = 16.0f;
        Zoom = FMath::Clamp(
            FMath::Min((localSize.X - margin * 2.0f) / atlasSize.X, (localSize.Y - margin * 2.0f) / atlasSize.Y),
            TextureAtlasCanvasPrivate::MinZoom, TextureAtlasCanvasPrivate::MaxZoom);
        ViewOffset = (localSize - FVector2f(atlasSize.X, atlasSize.Y) * Zoom) * 0.5f;
        LastFitAtlasSize = atlasSize;
        bFitRequested = false;
    }
}

FReply STextureAtlasCanvas::OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
    const FVector2f localPosition = MyGeometry.AbsoluteToLocal(MouseEvent.GetScreenSpacePosition());

    if (MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
    {
        const TSharedPtr<FTextureAtlasItem> hitItem = HitTest(LocalToAtlas(localPosition));
        SelectedItem = hitItem;
        OnItemSelected.ExecuteIfBound(hitItem);

        if (hitItem.IsValid())
        {
            bDragging = true;
            bDragMoved = false;
            DragStartLocal = localPosition;
            DragStartPosition = hitItem->Position;
        }
        return FReply::Handled().CaptureMouse(SharedThis(this)).SetUserFocus(SharedThis(this), EFocusCause::Mouse);
    }

    if (MouseEvent.GetEffectingButton() == EKeys::RightMouseButton || MouseEvent.GetEffectingButton() == EKeys::MiddleMouseButton)
    {
        bPanning = true;
        PanDistance = 0.0f;
        return FReply::Handled().CaptureMouse(SharedThis(this)).SetUserFocus(SharedThis(this), EFocusCause::Mouse);
    }

    return FReply::Unhandled();
}

FReply STextureAtlasCanvas::OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
    if (MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton && bDragging)
    {
        bDragging = false;
        if (bDragMoved)
        {
            OnItemsMoved.ExecuteIfBound();
        }
        return FReply::Handled().ReleaseMouseCapture();
    }

    if ((MouseEvent.GetEffectingButton() == EKeys::RightMouseButton || MouseEvent.GetEffectingButton() == EKeys::MiddleMouseButton) && bPanning)
    {
        bPanning = false;
        if (MouseEvent.GetEffectingButton() == EKeys::RightMouseButton && PanDistance < 4.0f && OnItemContextMenu.IsBound())
        {
            const FVector2f localPosition = MyGeometry.AbsoluteToLocal(MouseEvent.GetScreenSpacePosition());
            const TSharedPtr<FTextureAtlasItem> hitItem = HitTest(LocalToAtlas(localPosition));
            if (hitItem.IsValid())
            {
                SelectedItem = hitItem;
                OnItemSelected.ExecuteIfBound(hitItem);
            }

            const TSharedPtr<SWidget> menuContent = OnItemContextMenu.Execute(hitItem);
            if (menuContent.IsValid())
            {
                const FWidgetPath widgetPath = MouseEvent.GetEventPath() != nullptr ? *MouseEvent.GetEventPath() : FWidgetPath();
                FSlateApplication::Get().PushMenu(AsShared(), widgetPath, menuContent.ToSharedRef(), MouseEvent.GetScreenSpacePosition(), FPopupTransitionEffect(FPopupTransitionEffect::ContextMenu));
            }
        }
        return FReply::Handled().ReleaseMouseCapture();
    }

    return FReply::Unhandled();
}

FReply STextureAtlasCanvas::OnMouseMove(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
    if (bDragging && HasMouseCapture())
    {
        const TSharedPtr<FTextureAtlasItem> item = SelectedItem.Pin();
        if (item.IsValid())
        {
            const FVector2f localPosition = MyGeometry.AbsoluteToLocal(MouseEvent.GetScreenSpacePosition());
            const FVector2f delta = (localPosition - DragStartLocal) / Zoom;
            FIntPoint newPosition(FMath::RoundToInt(DragStartPosition.X + delta.X), FMath::RoundToInt(DragStartPosition.Y + delta.Y));
            if (MouseEvent.IsShiftDown())
            {
                const int32 snap = TextureAtlasCanvasPrivate::ShiftSnap;
                newPosition.X = FMath::RoundToInt(static_cast<float>(newPosition.X) / snap) * snap;
                newPosition.Y = FMath::RoundToInt(static_cast<float>(newPosition.Y) / snap) * snap;
            }

            newPosition = ClampPosition(*item, newPosition);
            if (newPosition != item->Position)
            {
                item->Position = newPosition;
                bDragMoved = true;
            }
        }
        return FReply::Handled();
    }

    if (bPanning && HasMouseCapture())
    {
        const FVector2f cursorDelta = MouseEvent.GetCursorDelta();
        const FVector2f localDelta = cursorDelta / FMath::Max(MyGeometry.Scale, KINDA_SMALL_NUMBER);
        ViewOffset += localDelta;
        PanDistance += localDelta.Size();
        return FReply::Handled();
    }

    return FReply::Unhandled();
}

FReply STextureAtlasCanvas::OnMouseWheel(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
    using namespace TextureAtlasCanvasPrivate;

    const FVector2f localPosition = MyGeometry.AbsoluteToLocal(MouseEvent.GetScreenSpacePosition());
    const float previousZoom = Zoom;
    Zoom = FMath::Clamp(MouseEvent.GetWheelDelta() > 0.0f ? Zoom * ZoomStep : Zoom / ZoomStep, MinZoom, MaxZoom);
    ViewOffset = localPosition - (localPosition - ViewOffset) * (Zoom / previousZoom);
    return FReply::Handled();
}

FReply STextureAtlasCanvas::OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent)
{
    const FKey key = InKeyEvent.GetKey();
    if (key == EKeys::F)
    {
        RequestFit();
        return FReply::Handled();
    }

    const TSharedPtr<FTextureAtlasItem> item = SelectedItem.Pin();
    if (!item.IsValid())
    {
        return FReply::Unhandled();
    }

    if (key == EKeys::Delete)
    {
        OnDeleteRequested.ExecuteIfBound(item);
        return FReply::Handled();
    }

    FIntPoint nudge = FIntPoint::ZeroValue;
    if (key == EKeys::Left)
    {
        nudge.X = -1;
    }
    else if (key == EKeys::Right)
    {
        nudge.X = 1;
    }
    else if (key == EKeys::Up)
    {
        nudge.Y = -1;
    }
    else if (key == EKeys::Down)
    {
        nudge.Y = 1;
    }

    if (nudge != FIntPoint::ZeroValue)
    {
        const int32 step = InKeyEvent.IsShiftDown() ? TextureAtlasCanvasPrivate::ShiftSnap : 1;
        item->Position = ClampPosition(*item, item->Position + nudge * step);
        OnItemsMoved.ExecuteIfBound();
        return FReply::Handled();
    }

    return FReply::Unhandled();
}

FCursorReply STextureAtlasCanvas::OnCursorQuery(const FGeometry& MyGeometry, const FPointerEvent& CursorEvent) const
{
    if (bPanning)
    {
        return FCursorReply::Cursor(EMouseCursor::GrabHandClosed);
    }
    if (bDragging)
    {
        return FCursorReply::Cursor(EMouseCursor::CardinalCross);
    }
    return FCursorReply::Cursor(EMouseCursor::Default);
}

bool STextureAtlasCanvas::SupportsKeyboardFocus() const
{
    return true;
}

void STextureAtlasCanvas::SetSelectedItem(TSharedPtr<FTextureAtlasItem> InItem)
{
    SelectedItem = InItem;
}

void STextureAtlasCanvas::RequestFit()
{
    bFitRequested = true;
}
