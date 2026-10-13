// Fill out your copyright notice in the Description page of Project Settings.

#include "Widgets/STextureToolkitBrowser.h"

#include "ContentBrowserDataSubsystem.h"
#include "ContentBrowserModule.h"
#include "Engine/Texture2D.h"
#include "IContentBrowserDataModule.h"
#include "IContentBrowserSingleton.h"
#include "Styling/AppStyle.h"
#include "WidgetBlueprint.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SSegmentedControl.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SSplitter.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "STextureToolkitBrowser"

void STextureToolkitBrowser::Construct(const FArguments& InArgs)
{
    OnGetContextMenu = InArgs._OnGetContextMenu;
    OnAssetsActivated = InArgs._OnAssetsActivated;
    CurrentInternalPath = TEXT("/Game");
    CurrentVirtualPath = IContentBrowserDataModule::Get().GetSubsystem()->ConvertInternalPathToVirtual(FName(*CurrentInternalPath)).ToString();

    FContentBrowserModule& contentBrowserModule = FModuleManager::LoadModuleChecked<FContentBrowserModule>(TEXT("ContentBrowser"));

    FPathPickerConfig pathPickerConfig;
    pathPickerConfig.DefaultPath = CurrentInternalPath;
    pathPickerConfig.bAllowContextMenu = false;
    pathPickerConfig.bFocusSearchBoxWhenOpened = false;
    pathPickerConfig.bOnPathSelectedPassesVirtualPaths = true;
    pathPickerConfig.OnPathSelected = FOnPathSelected::CreateSP(this, &STextureToolkitBrowser::HandlePathSelected);

    FAssetPickerConfig assetPickerConfig;
    assetPickerConfig.Filter = MakeFilter();
    assetPickerConfig.SelectionMode = ESelectionMode::Multi;
    assetPickerConfig.InitialAssetViewType = EAssetViewType::Tile;
    assetPickerConfig.bAllowNullSelection = false;
    assetPickerConfig.bFocusSearchBoxWhenOpened = false;
    assetPickerConfig.bCanShowFolders = false;
    assetPickerConfig.bAllowDragging = true;
    assetPickerConfig.bAllowRename = false;
    assetPickerConfig.bShowBottomToolbar = true;
    assetPickerConfig.bAddFilterUI = true;
    assetPickerConfig.bShowTypeInColumnView = false;
    assetPickerConfig.SaveSettingsName = TEXT("TextureToolkitBrowser");
    assetPickerConfig.GetCurrentSelectionDelegates.Add(&GetCurrentSelectionDelegate);
    assetPickerConfig.SetFilterDelegates.Add(&SetFilterDelegate);
    assetPickerConfig.RefreshAssetViewDelegates.Add(&RefreshAssetViewDelegate);
    assetPickerConfig.OnShouldFilterAsset = FOnShouldFilterAsset::CreateSP(this, &STextureToolkitBrowser::HandleShouldFilterAsset);
    assetPickerConfig.OnGetAssetContextMenu = FOnGetAssetContextMenu::CreateSP(this, &STextureToolkitBrowser::HandleAssetContextMenu);
    assetPickerConfig.OnAssetsActivated = FOnAssetsActivated::CreateSP(this, &STextureToolkitBrowser::HandleAssetsActivated);

    ChildSlot
    [
        SNew(SVerticalBox)

        + SVerticalBox::Slot()
        .AutoHeight()
        .Padding(4.0f)
        [
            SNew(SHorizontalBox)

            + SHorizontalBox::Slot()
            .AutoWidth()
            .VAlign(VAlign_Center)
            [
                SNew(SSegmentedControl<ETextureToolkitBrowserAssets>)
                .Value_Lambda([this]() { return AssetType; })
                .OnValueChanged(this, &STextureToolkitBrowser::HandleAssetTypeChanged)

                + SSegmentedControl<ETextureToolkitBrowserAssets>::Slot(ETextureToolkitBrowserAssets::Textures)
                .Text(LOCTEXT("Textures", "Textures"))
                .ToolTip(LOCTEXT("TexturesTooltip", "List Texture2D assets."))

                + SSegmentedControl<ETextureToolkitBrowserAssets>::Slot(ETextureToolkitBrowserAssets::WidgetBlueprints)
                .Text(LOCTEXT("Widgets", "Widgets"))
                .ToolTip(LOCTEXT("WidgetsTooltip", "List Widget Blueprints to gather their brush textures into an atlas."))
            ]

            + SHorizontalBox::Slot()
            .AutoWidth()
            .VAlign(VAlign_Center)
            .Padding(12.0f, 0.0f, 0.0f, 0.0f)
            [
                SNew(SCheckBox)
                .IsChecked_Lambda([this]() { return bRecursivePaths ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; })
                .OnCheckStateChanged(this, &STextureToolkitBrowser::HandleRecursiveChanged)
                [
                    SNew(STextBlock)
                    .Text(LOCTEXT("Subfolders", "Include subfolders"))
                ]
            ]

            + SHorizontalBox::Slot()
            .FillWidth(1.0f)
            .VAlign(VAlign_Center)
            .HAlign(HAlign_Right)
            .Padding(8.0f, 0.0f, 0.0f, 0.0f)
            [
                SNew(STextBlock)
                .Text_Lambda([this]() { return FText::FromString(CurrentInternalPath.IsEmpty() ? CurrentVirtualPath : CurrentInternalPath); })
                .ColorAndOpacity(FSlateColor::UseSubduedForeground())
                .OverflowPolicy(ETextOverflowPolicy::Ellipsis)
            ]
        ]

        + SVerticalBox::Slot()
        .FillHeight(1.0f)
        [
            SNew(SSplitter)
            .Orientation(Orient_Horizontal)

            + SSplitter::Slot()
            .Value(0.35f)
            [
                SNew(SBorder)
                .BorderImage(FAppStyle::GetBrush("Brushes.Recessed"))
                .Padding(2.0f)
                [
                    contentBrowserModule.Get().CreatePathPicker(pathPickerConfig)
                ]
            ]

            + SSplitter::Slot()
            .Value(0.65f)
            [
                contentBrowserModule.Get().CreateAssetPicker(assetPickerConfig)
            ]
        ]

        + SVerticalBox::Slot()
        .AutoHeight()
        .Padding(4.0f, 2.0f)
        [
            SNew(STextBlock)
            .Text(LOCTEXT("Hint", "Ctrl+A selects all. Right click for actions, double click adds to the active mode. Assets can be dragged into the panels."))
            .ColorAndOpacity(FSlateColor::UseSubduedForeground())
            .AutoWrapText(true)
        ]
    ];
}

FARFilter STextureToolkitBrowser::MakeFilter() const
{
    FARFilter filter;
    if (AssetType == ETextureToolkitBrowserAssets::Textures)
    {
        filter.ClassPaths.Add(UTexture2D::StaticClass()->GetClassPathName());
    }
    else
    {
        filter.ClassPaths.Add(UWidgetBlueprint::StaticClass()->GetClassPathName());
    }
    filter.bRecursiveClasses = true;

    if (!CurrentVirtualPath.IsEmpty())
    {
        filter.PackagePaths.Add(FName(*CurrentVirtualPath));
    }
    filter.bRecursivePaths = true;
    return filter;
}

void STextureToolkitBrowser::ApplyFilter()
{
    SetFilterDelegate.ExecuteIfBound(MakeFilter());
    RefreshAssetViewDelegate.ExecuteIfBound(true);
}

void STextureToolkitBrowser::HandlePathSelected(const FString& InVirtualPath)
{
    CurrentVirtualPath = InVirtualPath;

    FString internalPath;
    const EContentBrowserPathType pathType = IContentBrowserDataModule::Get().GetSubsystem()->TryConvertVirtualPath(InVirtualPath, internalPath);
    CurrentInternalPath = pathType == EContentBrowserPathType::Internal ? internalPath : FString();

    ApplyFilter();
}

bool STextureToolkitBrowser::HandleShouldFilterAsset(const FAssetData& InAssetData) const
{
    if (bRecursivePaths || CurrentInternalPath.IsEmpty())
    {
        return false;
    }
    return InAssetData.PackagePath.ToString() != CurrentInternalPath;
}

TSharedPtr<SWidget> STextureToolkitBrowser::HandleAssetContextMenu(const TArray<FAssetData>& InAssets)
{
    if (OnGetContextMenu.IsBound() && InAssets.Num() > 0)
    {
        return OnGetContextMenu.Execute(InAssets);
    }
    return nullptr;
}

void STextureToolkitBrowser::HandleAssetsActivated(const TArray<FAssetData>& InAssets, EAssetTypeActivationMethod::Type InMethod)
{
    if (InMethod != EAssetTypeActivationMethod::Previewed)
    {
        OnAssetsActivated.ExecuteIfBound(InAssets);
    }
}

void STextureToolkitBrowser::HandleAssetTypeChanged(ETextureToolkitBrowserAssets InAssetType)
{
    if (AssetType != InAssetType)
    {
        AssetType = InAssetType;
        ApplyFilter();
    }
}

void STextureToolkitBrowser::HandleRecursiveChanged(ECheckBoxState InState)
{
    bRecursivePaths = InState == ECheckBoxState::Checked;
    ApplyFilter();
}

TArray<FAssetData> STextureToolkitBrowser::GetSelectedAssets() const
{
    if (GetCurrentSelectionDelegate.IsBound())
    {
        return GetCurrentSelectionDelegate.Execute();
    }
    return TArray<FAssetData>();
}

#undef LOCTEXT_NAMESPACE
