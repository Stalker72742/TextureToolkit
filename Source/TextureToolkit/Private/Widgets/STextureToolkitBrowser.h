// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AssetRegistry/ARFilter.h"
#include "ContentBrowserDelegates.h"
#include "Widgets/SCompoundWidget.h"

#include "Core/TextureToolkitTypes.h"

/** @brief Builds the context menu for the selected browser assets. */
DECLARE_DELEGATE_RetVal_OneParam(TSharedPtr<SWidget>, FOnTextureToolkitBrowserContextMenu, const TArray<FAssetData>& /*InAssets*/);

/** @brief Fired on double click or Enter on browser assets. */
DECLARE_DELEGATE_OneParam(FOnTextureToolkitBrowserActivated, const TArray<FAssetData>& /*InAssets*/);

/**
 * @brief Mini content browser of the toolkit: folder tree, searchable multi-select asset view and a context menu.
 * @note Lists Texture2D assets or Widget Blueprints (to gather their brush textures into an atlas).
 */
class STextureToolkitBrowser : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(STextureToolkitBrowser) {}
        /** @brief Context menu provider. */
        SLATE_EVENT(FOnTextureToolkitBrowserContextMenu, OnGetContextMenu)
        /** @brief Activation handler. */
        SLATE_EVENT(FOnTextureToolkitBrowserActivated, OnAssetsActivated)
    SLATE_END_ARGS()

    /**
     * @brief Builds the widget.
     * @param InArgs Slate arguments.
     */
    void Construct(const FArguments& InArgs);

protected:
    // --- Runtime fields ---

    /** @brief Context menu provider. */
    FOnTextureToolkitBrowserContextMenu OnGetContextMenu;

    /** @brief Activation handler. */
    FOnTextureToolkitBrowserActivated OnAssetsActivated;

    /** @brief Bound by the asset picker, returns the current selection. */
    FGetCurrentSelectionDelegate GetCurrentSelectionDelegate;

    /** @brief Bound by the asset picker, replaces its filter. */
    FSetARFilterDelegate SetFilterDelegate;

    /** @brief Bound by the asset picker, refreshes its items. */
    FRefreshAssetViewDelegate RefreshAssetViewDelegate;

    /**
     * @brief Virtual path of the folder selected in the path tree ("/All/Game/UI" or "/Game/UI").
     * @note The asset picker filters by virtual paths, not by package paths.
     */
    FString CurrentVirtualPath;

    /** @brief Package path of the selected folder ("/Game/UI"), empty for purely virtual folders such as "/All/Plugins". */
    FString CurrentInternalPath;

    /** @brief Listed asset type. */
    ETextureToolkitBrowserAssets AssetType = ETextureToolkitBrowserAssets::Textures;

    /** @brief Includes sub folders of the selected folder. */
    bool bRecursivePaths = true;

protected:
    // --- Handlers ---

    /** @brief Path tree selection handler, receives a virtual path. */
    void HandlePathSelected(const FString& InVirtualPath);

    /** @brief Hides assets outside the selected folder when sub folders are excluded. */
    bool HandleShouldFilterAsset(const FAssetData& InAssetData) const;

    /** @brief Asset view context menu handler. */
    TSharedPtr<SWidget> HandleAssetContextMenu(const TArray<FAssetData>& InAssets);

    /** @brief Asset view activation handler. */
    void HandleAssetsActivated(const TArray<FAssetData>& InAssets, EAssetTypeActivationMethod::Type InMethod);

    /** @brief Asset type switch handler. */
    void HandleAssetTypeChanged(ETextureToolkitBrowserAssets InAssetType);

    /** @brief Sub folder checkbox handler. */
    void HandleRecursiveChanged(ECheckBoxState InState);

protected:
    // --- Helpers ---

    /** @brief Builds the registry filter for the current folder and asset type. */
    FARFilter MakeFilter() const;

    /** @brief Pushes the current filter to the asset picker. */
    void ApplyFilter();

public:
    // --- Public functions ---

    /** @brief Returns the assets selected in the asset view. */
    TArray<FAssetData> GetSelectedAssets() const;

public:
    // --- Const getters ---

    /** @brief Returns the listed asset type. */
    FORCEINLINE ETextureToolkitBrowserAssets GetAssetType() const { return AssetType; }
};
