// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SLeafWidget.h"

#include "Core/TextureToolkitTypes.h"

/** @brief Fired when the canvas selection changes. */
DECLARE_DELEGATE_OneParam(FOnTextureAtlasItemSelected, TSharedPtr<FTextureAtlasItem> /*InItem*/);

/** @brief Fired after items were moved with the mouse or the keyboard. */
DECLARE_DELEGATE(FOnTextureAtlasItemsMoved);

/** @brief Builds the context menu for an item (null when clicking empty space). */
DECLARE_DELEGATE_RetVal_OneParam(TSharedPtr<SWidget>, FOnTextureAtlasItemContextMenu, TSharedPtr<FTextureAtlasItem> /*InItem*/);

/** @brief Fired when Delete is pressed on the selected item. */
DECLARE_DELEGATE_OneParam(FOnTextureAtlasItemDelete, TSharedPtr<FTextureAtlasItem> /*InItem*/);

/**
 * @brief Interactive atlas preview.
 * @note Left drag moves sprites (Shift snaps to 16 px), arrows nudge, right or middle drag pans, wheel zooms,
 *       right click opens the context menu, F fits the view, Delete removes the selected sprite.
 */
class STextureAtlasCanvas : public SLeafWidget
{
public:
    SLATE_BEGIN_ARGS(STextureAtlasCanvas)
        : _Items(nullptr)
        , _BaseOccupied(nullptr)
        , _Padding(0)
        {}
        /** @brief Items owned by the panel. */
        SLATE_ARGUMENT(const TArray<TSharedPtr<FTextureAtlasItem>>*, Items)
        /** @brief Occupied regions of the base atlas owned by the panel. */
        SLATE_ARGUMENT(const TArray<FIntRect>*, BaseOccupied)
        /** @brief Current atlas size. */
        SLATE_ATTRIBUTE(FIntPoint, AtlasSize)
        /** @brief Padding per side. */
        SLATE_ATTRIBUTE(int32, Padding)
        /** @brief Brush of the base atlas texture, may be null. */
        SLATE_ATTRIBUTE(const FSlateBrush*, BaseAtlasBrush)
        /** @brief Size of the base atlas texture. */
        SLATE_ATTRIBUTE(FIntPoint, BaseAtlasSize)
        /** @brief Selection handler. */
        SLATE_EVENT(FOnTextureAtlasItemSelected, OnItemSelected)
        /** @brief Move handler. */
        SLATE_EVENT(FOnTextureAtlasItemsMoved, OnItemsMoved)
        /** @brief Context menu provider. */
        SLATE_EVENT(FOnTextureAtlasItemContextMenu, OnItemContextMenu)
        /** @brief Delete handler. */
        SLATE_EVENT(FOnTextureAtlasItemDelete, OnDeleteRequested)
    SLATE_END_ARGS()

    /**
     * @brief Builds the widget.
     * @param InArgs Slate arguments.
     */
    void Construct(const FArguments& InArgs);

protected:
    // --- Runtime fields ---

    /** @brief Items owned by the panel. */
    const TArray<TSharedPtr<FTextureAtlasItem>>* Items = nullptr;

    /** @brief Occupied base atlas regions owned by the panel. */
    const TArray<FIntRect>* BaseOccupied = nullptr;

    /** @brief Current atlas size. */
    TAttribute<FIntPoint> AtlasSize;

    /** @brief Padding per side. */
    TAttribute<int32> Padding;

    /** @brief Base atlas brush. */
    TAttribute<const FSlateBrush*> BaseAtlasBrush;

    /** @brief Base atlas size. */
    TAttribute<FIntPoint> BaseAtlasSize;

    /** @brief Selection handler. */
    FOnTextureAtlasItemSelected OnItemSelected;

    /** @brief Move handler. */
    FOnTextureAtlasItemsMoved OnItemsMoved;

    /** @brief Context menu provider. */
    FOnTextureAtlasItemContextMenu OnItemContextMenu;

    /** @brief Delete handler. */
    FOnTextureAtlasItemDelete OnDeleteRequested;

    /** @brief Selected item. */
    TWeakPtr<FTextureAtlasItem> SelectedItem;

    /** @brief Local position of the atlas origin. */
    FVector2f ViewOffset = FVector2f::ZeroVector;

    /** @brief Screen pixels per atlas pixel. */
    float Zoom = 1.0f;

    /** @brief Fit is applied on the next tick. */
    bool bFitRequested = true;

    /** @brief Atlas size at the last fit. */
    FIntPoint LastFitAtlasSize = FIntPoint::ZeroValue;

    /** @brief True while an item is dragged. */
    bool bDragging = false;

    /** @brief True when the dragged item moved. */
    bool bDragMoved = false;

    /** @brief True while the view is panned. */
    bool bPanning = false;

    /** @brief Distance panned since the button press, used to tell a click from a drag. */
    float PanDistance = 0.0f;

    /** @brief Local mouse position at drag start. */
    FVector2f DragStartLocal = FVector2f::ZeroVector;

    /** @brief Item position at drag start. */
    FIntPoint DragStartPosition = FIntPoint::ZeroValue;

protected:
    // --- Helpers ---

    /** @brief Converts atlas pixels to local widget space. */
    FVector2f AtlasToLocal(const FVector2f& InAtlasPosition) const;

    /** @brief Converts local widget space to atlas pixels. */
    FVector2f LocalToAtlas(const FVector2f& InLocalPosition) const;

    /** @brief Returns the top-most item under an atlas position. */
    TSharedPtr<FTextureAtlasItem> HitTest(const FVector2f& InAtlasPosition) const;

    /** @brief Clamps an item position to the atlas. */
    FIntPoint ClampPosition(const FTextureAtlasItem& InItem, const FIntPoint& InPosition) const;

    /** @brief Draws a rectangle outline in local space. */
    void DrawOutline(FSlateWindowElementList& OutDrawElements, int32 InLayerId, const FGeometry& InGeometry, const FVector2f& InMin, const FVector2f& InMax, const FLinearColor& InColor, float InThickness) const;

public:
    // --- SWidget overrides ---

    virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;
    virtual FVector2D ComputeDesiredSize(float LayoutScaleMultiplier) const override;
    virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;
    virtual FReply OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
    virtual FReply OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
    virtual FReply OnMouseMove(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
    virtual FReply OnMouseWheel(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
    virtual FReply OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent) override;
    virtual FCursorReply OnCursorQuery(const FGeometry& MyGeometry, const FPointerEvent& CursorEvent) const override;
    virtual bool SupportsKeyboardFocus() const override;

public:
    // --- Public functions ---

    /**
     * @brief Selects an item without firing OnItemSelected.
     * @param InItem Item to select, null clears the selection.
     */
    void SetSelectedItem(TSharedPtr<FTextureAtlasItem> InItem);

    /** @brief Fits the atlas into the view on the next tick. */
    void RequestFit();

    /**
     * @brief Finds items that overlap each other or base content (padding included) or leave the atlas.
     * @param InItems Items to check.
     * @param InOccupied Occupied base atlas regions.
     * @param InAtlasSize Atlas size.
     * @param InPadding Padding per side.
     * @param OutInvalid Invalid items.
     */
    static void FindInvalidItems(const TArray<TSharedPtr<FTextureAtlasItem>>& InItems, const TArray<FIntRect>& InOccupied, const FIntPoint& InAtlasSize, int32 InPadding, TSet<const FTextureAtlasItem*>& OutInvalid);

public:
    // --- Const getters ---

    /** @brief Returns the selected item. */
    FORCEINLINE TSharedPtr<FTextureAtlasItem> GetSelectedItem() const { return SelectedItem.Pin(); }

    /** @brief Returns the current zoom. */
    FORCEINLINE float GetZoom() const { return Zoom; }
};
