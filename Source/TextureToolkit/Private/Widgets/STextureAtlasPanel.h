// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AssetRegistry/AssetData.h"
#include "Styling/SlateBrush.h"
#include "UObject/GCObject.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Views/SListView.h"

#include "Core/TextureToolkitTypes.h"

class IStructureDetailsView;
class FStructOnScope;
class STextureAtlasCanvas;
class UPaperSprite;
class UTexture2D;
struct FPropertyChangedEvent;

/**
 * @brief Atlas mode: interactive atlas layout, atlas texture and PaperSprite generation, base atlas extension
 *        and replacement of texture brushes in widgets with the generated sprites.
 */
class STextureAtlasPanel : public SCompoundWidget, public FGCObject
{
public:
    SLATE_BEGIN_ARGS(STextureAtlasPanel) {}
    SLATE_END_ARGS()

    /**
     * @brief Builds the widget.
     * @param InArgs Slate arguments.
     */
    void Construct(const FArguments& InArgs);

    /** @brief Destructor, defined where UPaperSprite is a complete type. */
    virtual ~STextureAtlasPanel() override;

protected:
    // --- Runtime fields ---

    /** @brief Sprites placed on the atlas. */
    TArray<TSharedPtr<FTextureAtlasItem>> Items;

    /** @brief List of Items. */
    TSharedPtr<SListView<TSharedPtr<FTextureAtlasItem>>> ItemList;

    /** @brief Interactive preview. */
    TSharedPtr<STextureAtlasCanvas> Canvas;

    /** @brief Layout and output settings. */
    FTextureAtlasOptions Options;

    /** @brief Options before the last change, used to detect what changed. */
    FTextureAtlasOptions PreviousOptions;

    /** @brief Scope binding Options to the details view. */
    TSharedPtr<FStructOnScope> OptionsScope;

    /** @brief Details view of Options. */
    TSharedPtr<IStructureDetailsView> OptionsView;

    /** @brief Current atlas size. */
    FIntPoint AtlasSize = FIntPoint(1024, 1024);

    /** @brief Existing atlas extended with new sprites. */
    TObjectPtr<UTexture2D> BaseAtlas = nullptr;

    /** @brief Source size of BaseAtlas. */
    FIntPoint BaseAtlasSize = FIntPoint::ZeroValue;

    /** @brief Brush drawing BaseAtlas on the canvas. */
    FSlateBrush BaseAtlasBrush;

    /** @brief Regions of BaseAtlas that already contain pixels or sprites. */
    TArray<FIntRect> BaseOccupied;

    /** @brief Sprites that use BaseAtlas. */
    TArray<TObjectPtr<UPaperSprite>> BaseSprites;

    /** @brief Widget Blueprints whose brushes are switched to the generated sprites after generation. */
    TArray<TSharedPtr<FAssetData>> AtlasifyWidgets;

    /** @brief List of AtlasifyWidgets. */
    TSharedPtr<SListView<TSharedPtr<FAssetData>>> WidgetList;

    /** @brief Atlas written by the last generation. */
    TObjectPtr<UTexture2D> GeneratedAtlas = nullptr;

    /** @brief Source texture to sprite written by the last generation. */
    TMap<TObjectPtr<UTexture2D>, TObjectPtr<UPaperSprite>> GeneratedSprites;

    /** @brief Last status message. */
    FText StatusText;

protected:
    // --- Handlers ---

    /** @brief Creates a sprite list row. */
    TSharedRef<ITableRow> HandleGenerateItemRow(TSharedPtr<FTextureAtlasItem> InItem, const TSharedRef<STableViewBase>& InOwnerTable);

    /** @brief Creates a widget list row. */
    TSharedRef<ITableRow> HandleGenerateWidgetRow(TSharedPtr<FAssetData> InAsset, const TSharedRef<STableViewBase>& InOwnerTable);

    /** @brief Syncs list selection to the canvas. */
    void HandleItemSelectionChanged(TSharedPtr<FTextureAtlasItem> InItem, ESelectInfo::Type InSelectInfo);

    /** @brief Syncs canvas selection to the list. */
    void HandleCanvasItemSelected(TSharedPtr<FTextureAtlasItem> InItem);

    /** @brief Canvas move handler. */
    void HandleCanvasItemsMoved();

    /** @brief Canvas context menu. */
    TSharedPtr<SWidget> HandleCanvasContextMenu(TSharedPtr<FTextureAtlasItem> InItem);

    /** @brief Sprite list context menu. */
    TSharedPtr<SWidget> HandleItemListContextMenu();

    /** @brief Canvas delete handler. */
    void HandleDeleteRequested(TSharedPtr<FTextureAtlasItem> InItem);

    /** @brief Reacts to option changes (trim, padding, size). */
    void HandleOptionsChanged(const FPropertyChangedEvent& InEvent);

    /** @brief Auto Arrange button handler. */
    FReply HandleAutoArrangeClicked();

    /** @brief Fit button handler. */
    FReply HandleFitClicked();

    /** @brief Removes selected sprites. */
    FReply HandleRemoveSelectedClicked();

    /** @brief Removes every sprite. */
    FReply HandleClearClicked();

    /** @brief Generate button handler. */
    FReply HandleGenerateClicked();

    /** @brief Replace All References button handler. */
    FReply HandleReplaceReferencesClicked();

    /** @brief Clears the base atlas. */
    FReply HandleClearBaseClicked();

    /** @brief Removes selected widgets from the Atlasify list. */
    FReply HandleRemoveWidgetsClicked();

    /** @brief Clears the Atlasify list. */
    FReply HandleClearWidgetsClicked();

protected:
    // --- Helpers ---

    /**
     * @brief Repacks every unlocked sprite.
     * @return False when the sprites do not fit.
     */
    bool AutoArrange();

    /**
     * @brief Places new sprites into free space without moving the existing ones, falls back to AutoArrange.
     * @param InNewItems Items that were just added.
     */
    void PlaceNewItems(const TArray<TSharedPtr<FTextureAtlasItem>>& InNewItems);

    /** @brief Applies the trim option to every item. */
    void ApplyTrimToItems(bool bInRecomputeBounds);

    /** @brief Recomputes occupied regions and sprites of the base atlas. */
    void AnalyzeBaseAtlas();

    /** @brief Removes items and refreshes the views. */
    void RemoveItems(const TArray<TSharedPtr<FTextureAtlasItem>>& InItems);

    /** @brief Refreshes both lists. */
    void RefreshLists();

    /** @brief Pushes externally changed Options into the details view. */
    void RefreshOptionsView();

    /** @brief Sets the scale of items. */
    void SetItemsScale(const TArray<TSharedPtr<FTextureAtlasItem>>& InItems, float InScale);

    /** @brief Toggles the lock of items. */
    void SetItemsLocked(const TArray<TSharedPtr<FTextureAtlasItem>>& InItems, bool bInLocked);

    /** @brief Fills a menu with actions for items. */
    void FillItemMenu(class FMenuBuilder& InMenuBuilder, const TArray<TSharedPtr<FTextureAtlasItem>>& InItems);

    /** @brief Returns the selected items of the list. */
    TArray<TSharedPtr<FTextureAtlasItem>> GetSelectedItems() const;

    /** @brief Finds the item of a texture. */
    TSharedPtr<FTextureAtlasItem> FindItem(const UTexture2D* InTexture) const;

    /** @brief Builds the old texture to new sprite map from the last generation. */
    TMap<UObject*, UObject*> BuildReplacementMap() const;

    /** @brief Packages that reference replacement must never touch (atlas, sprites, source textures). */
    TSet<FName> GetProtectedPackages() const;

    /** @brief Switches brushes of the Atlasify widgets to the generated sprites. */
    void ReplaceInAtlasifyWidgets();

    /** @brief Builds the selected sprite editor row. */
    TSharedRef<SWidget> BuildSelectedItemRow();

    /** @brief Builds the base atlas row. */
    TSharedRef<SWidget> BuildBaseAtlasRow();

    /** @brief Returns the atlas statistics line. */
    FText GetAtlasInfoText() const;

    /** @brief Returns the base atlas description. */
    FText GetBaseAtlasText() const;

    /** @brief Returns the selected sprite or null. */
    TSharedPtr<FTextureAtlasItem> GetSelectedItem() const;

public:
    // --- SWidget overrides ---

    virtual FReply OnDragOver(const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent) override;
    virtual FReply OnDrop(const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent) override;

public:
    // --- FGCObject ---

    virtual void AddReferencedObjects(FReferenceCollector& Collector) override;
    virtual FString GetReferencerName() const override;

public:
    // --- Public functions ---

    /**
     * @brief Adds textures as sprites, duplicates and the base atlas itself are ignored.
     * @param InTextures Textures to add.
     */
    void AddTextures(const TArray<UTexture2D*>& InTextures);

    /**
     * @brief Uses an existing atlas as the background: its content is kept and new sprites fill the free space.
     * @param InTexture Atlas texture, null clears it.
     */
    void SetBaseAtlas(UTexture2D* InTexture);

    /**
     * @brief Adds Widget Blueprints: their brush textures are added as sprites and their brushes are switched to the sprites after generation.
     * @param InAssets Widget Blueprint assets.
     */
    void AddWidgets(const TArray<FAssetData>& InAssets);
};
