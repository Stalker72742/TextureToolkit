// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/GCObject.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Views/SListView.h"

#include "Core/TextureCompressor.h"
#include "Core/TextureToolkitTypes.h"

class IStructureDetailsView;
class FStructOnScope;
class UTexture2D;
struct FPropertyChangedEvent;

/**
 * @brief One texture listed in Compress mode.
 */
struct FTextureCompressEntry
{
    /** @brief Listed texture. */
    TObjectPtr<UTexture2D> Texture = nullptr;

    /** @brief What Apply would do with it. */
    FTextureCompressPlan Plan;
};

/**
 * @brief Compress mode: downscales textures (max size, LOD bias or source resample) and overrides format settings.
 */
class STextureCompressPanel : public SCompoundWidget, public FGCObject
{
public:
    SLATE_BEGIN_ARGS(STextureCompressPanel) {}
    SLATE_END_ARGS()

    /**
     * @brief Builds the widget.
     * @param InArgs Slate arguments.
     */
    void Construct(const FArguments& InArgs);

protected:
    // --- Runtime fields ---

    /** @brief Settings edited in the details view. */
    FTextureCompressOptions Options;

    /** @brief Scope binding Options to the details view. */
    TSharedPtr<FStructOnScope> OptionsScope;

    /** @brief Details view of Options. */
    TSharedPtr<IStructureDetailsView> OptionsView;

    /** @brief Listed textures. */
    TArray<TSharedPtr<FTextureCompressEntry>> Entries;

    /** @brief List of Entries. */
    TSharedPtr<SListView<TSharedPtr<FTextureCompressEntry>>> ListView;

protected:
    // --- Handlers ---

    /** @brief Creates a list row. */
    TSharedRef<ITableRow> HandleGenerateRow(TSharedPtr<FTextureCompressEntry> InEntry, const TSharedRef<STableViewBase>& InOwnerTable);

    /** @brief Recomputes plans after an option change. */
    void HandleOptionsChanged(const FPropertyChangedEvent& InEvent);

    /** @brief Builds the list context menu. */
    TSharedPtr<SWidget> HandleListContextMenu();

    /** @brief Opens the texture editor on double click. */
    void HandleListDoubleClick(TSharedPtr<FTextureCompressEntry> InEntry);

    /** @brief Apply button handler. */
    FReply HandleApplyClicked();

    /** @brief Removes selected entries. */
    FReply HandleRemoveSelectedClicked();

    /** @brief Removes entries without changes. */
    FReply HandleRemoveUnchangedClicked();

    /** @brief Removes every entry. */
    FReply HandleClearClicked();

    /** @brief Recomputes plans. */
    FReply HandleRefreshClicked();

protected:
    // --- Helpers ---

    /** @brief Recomputes every plan and refreshes the list. */
    void RefreshPlans();

    /** @brief Returns the totals line. */
    FText GetSummaryText() const;

    /** @brief Returns the selected textures. */
    TArray<UObject*> GetSelectedTextures() const;

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
     * @brief Adds textures to the list, duplicates are ignored.
     * @param InTextures Textures to add.
     */
    void AddTextures(const TArray<UTexture2D*>& InTextures);
};
