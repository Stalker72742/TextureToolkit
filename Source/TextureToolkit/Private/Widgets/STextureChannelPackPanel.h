// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Styling/SlateBrush.h"
#include "UObject/GCObject.h"
#include "Widgets/SCompoundWidget.h"

#include "Core/TextureChannelPacker.h"
#include "Core/TextureToolkitTypes.h"

class IStructureDetailsView;
class FStructOnScope;
class UTexture2D;
struct FAssetData;

/**
 * @brief Channel Pack mode: builds ORM-like masks from separate textures, transfers channels and splits textures.
 */
class STextureChannelPackPanel : public SCompoundWidget, public FGCObject
{
public:
    SLATE_BEGIN_ARGS(STextureChannelPackPanel) {}
    SLATE_END_ARGS()

    /**
     * @brief Builds the widget.
     * @param InArgs Slate arguments.
     */
    void Construct(const FArguments& InArgs);

protected:
    // --- Runtime fields ---

    /** @brief Output channels R, G, B, A. */
    TArray<FTextureChannelInput> Inputs;

    /** @brief Active preset. */
    EChannelPackPreset Preset = EChannelPackPreset::ORM;

    /** @brief Output settings edited in the details view. */
    FTextureChannelPackOptions Options;

    /** @brief Scope binding Options to the details view. */
    TSharedPtr<FStructOnScope> OptionsScope;

    /** @brief Details view of Options. */
    TSharedPtr<IStructureDetailsView> OptionsView;

    /** @brief Transient texture with the packed preview. */
    TObjectPtr<UTexture2D> PreviewTexture = nullptr;

    /** @brief Brush drawing PreviewTexture. */
    FSlateBrush PreviewBrush;

    /** @brief Previewed channel, INDEX_NONE shows RGB. */
    int32 PreviewChannel = INDEX_NONE;

    /** @brief Inputs resampled to the preview size. */
    FTextureChannelImageCache PreviewCache;

    /** @brief True when the preview must be rebuilt on the next tick. */
    bool bPreviewDirty = true;

    /** @brief Texture split by the Split Channels section. */
    TObjectPtr<UTexture2D> SplitTexture = nullptr;

    /** @brief Channels written by Split Channels: bit 0 = R ... bit 3 = A. */
    uint8 SplitMask = 0x7;

protected:
    // --- Handlers ---

    /** @brief Marks the preview dirty after an output option change. */
    void HandleOptionsChanged(const FPropertyChangedEvent& InEvent);

    /** @brief Create button handler. */
    FReply HandleCreateClicked();

    /** @brief Split button handler. */
    FReply HandleSplitClicked();

    /** @brief Clears every input texture. */
    FReply HandleClearInputsClicked();

    /** @brief Drops cached preview images and rebuilds the preview. */
    FReply HandleRefreshPreviewClicked();

protected:
    // --- Helpers ---

    /** @brief Builds one R / G / B / A row. */
    TSharedRef<SWidget> BuildChannelRow(int32 InChannelIndex);

    /** @brief Builds the preset drop down menu. */
    TSharedRef<SWidget> BuildPresetMenu();

    /** @brief Builds the Split Channels section. */
    TSharedRef<SWidget> BuildSplitSection();

    /** @brief Rebuilds the preview texture. */
    void UpdatePreview();

    /** @brief Returns the line describing the output asset. */
    FText GetOutputInfoText() const;

    /** @brief Returns the display name of the active preset. */
    FText GetPresetText() const;

public:
    // --- SWidget overrides ---

    virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;
    virtual FReply OnDragOver(const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent) override;
    virtual FReply OnDrop(const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent) override;

public:
    // --- FGCObject ---

    virtual void AddReferencedObjects(FReferenceCollector& Collector) override;
    virtual FString GetReferencerName() const override;

public:
    // --- Public functions ---

    /**
     * @brief Applies a preset (labels, constants, invert flags), textures are kept.
     * @param InPreset Preset to apply.
     */
    void SetPreset(EChannelPackPreset InPreset);

    /**
     * @brief Assigns a texture to an output channel.
     * @param InChannelIndex 0 = R, 1 = G, 2 = B, 3 = A.
     * @param InTexture Texture, null clears the slot.
     */
    void SetInputTexture(int32 InChannelIndex, UTexture2D* InTexture);

    /**
     * @brief Copies R, G and B from one texture (channel transfer), switching to the Custom preset.
     * @param InTexture Source of the RGB channels.
     */
    void SetRGBSource(UTexture2D* InTexture);

    /**
     * @brief Assigns textures by name (_AO, _Roughness, ...) and fills empty slots with the rest.
     * @param InTextures Textures to assign.
     */
    void AutoAssignTextures(const TArray<UTexture2D*>& InTextures);

    /**
     * @brief Sets the texture of the Split Channels section.
     * @param InTexture Texture to split.
     */
    void SetSplitTexture(UTexture2D* InTexture);
};
