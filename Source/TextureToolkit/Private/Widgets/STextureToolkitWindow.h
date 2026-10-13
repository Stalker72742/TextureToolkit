// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"

#include "Core/TextureToolkitTypes.h"

class STextureToolkitBrowser;
class STextureCompressPanel;
class STextureChannelPackPanel;
class STextureAtlasPanel;
class UTexture2D;
struct FAssetData;

/**
 * @brief Root widget of the Texture Toolkit tab: mode switch, texture browser and the mode panels.
 */
class STextureToolkitWindow : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(STextureToolkitWindow) {}
    SLATE_END_ARGS()

    /**
     * @brief Builds the widget.
     * @param InArgs Slate arguments.
     */
    void Construct(const FArguments& InArgs);

protected:
    // --- Runtime fields ---

    /** @brief Active mode. */
    ETextureToolkitMode Mode = ETextureToolkitMode::Compress;

    /** @brief Texture browser. */
    TSharedPtr<STextureToolkitBrowser> Browser;

    /** @brief Compress mode panel. */
    TSharedPtr<STextureCompressPanel> CompressPanel;

    /** @brief Channel Pack mode panel. */
    TSharedPtr<STextureChannelPackPanel> ChannelPackPanel;

    /** @brief Atlas mode panel. */
    TSharedPtr<STextureAtlasPanel> AtlasPanel;

protected:
    // --- Handlers ---

    /** @brief Builds the browser context menu. */
    TSharedPtr<SWidget> HandleBrowserContextMenu(const TArray<FAssetData>& InAssets);

    /** @brief Adds activated browser assets to the active mode. */
    void HandleBrowserActivated(const TArray<FAssetData>& InAssets);

    /** @brief Adds the browser selection to the active mode. */
    FReply HandleAddSelectedClicked();

protected:
    // --- Helpers ---

    /**
     * @brief Switches the visible panel.
     * @param InMode Mode to show.
     */
    void SetMode(ETextureToolkitMode InMode);

    /**
     * @brief Adds assets to a mode and shows it.
     * @param InMode Target mode.
     * @param InAssets Textures or Widget Blueprints.
     */
    void AddToMode(ETextureToolkitMode InMode, const TArray<FAssetData>& InAssets);

    /** @brief Returns the display name of a mode. */
    static FText GetModeName(ETextureToolkitMode InMode);
};
