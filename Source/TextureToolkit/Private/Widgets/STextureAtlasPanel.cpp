// Fill out your copyright notice in the Description page of Project Settings.

#include "Widgets/STextureAtlasPanel.h"

#include "Engine/Texture2D.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "IDetailsView.h"
#include "IStructureDetailsView.h"
#include "Misc/MessageDialog.h"
#include "Misc/ScopedSlowTask.h"
#include "PaperSprite.h"
#include "PropertyCustomizationHelpers.h"
#include "Styling/AppStyle.h"
#include "ThumbnailRendering/ThumbnailManager.h"
#include "UObject/StructOnScope.h"
#include "WidgetBlueprint.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SNumericEntryBox.h"
#include "Widgets/Input/SSpinBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScaleBox.h"
#include "Widgets/Layout/SSplitter.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Views/STableRow.h"

#include "Core/TextureAtlasBuilder.h"
#include "Core/TextureAtlasPacker.h"
#include "Core/TextureToolkitAssetUtils.h"
#include "Core/TextureToolkitImageUtils.h"
#include "Widgets/STextureAtlasCanvas.h"
#include "Widgets/TextureToolkitWidgetUtils.h"

#define LOCTEXT_NAMESPACE "STextureAtlasPanel"

namespace TextureAtlasPanelPrivate
{
    TSharedRef<SWidget> MakeIconButton(const FName& InIcon, const FText& InTooltip, FOnClicked InOnClicked)
    {
        return SNew(SButton)
            .ButtonStyle(FAppStyle::Get(), "SimpleButton")
            .ToolTipText(InTooltip)
            .OnClicked(InOnClicked)
            [
                SNew(SImage)
                .Image(FAppStyle::GetBrush(InIcon))
                .ColorAndOpacity(FSlateColor::UseForeground())
            ];
    }

    FText GetItemInfo(const FTextureAtlasItem& InItem)
    {
        const FIntPoint size = InItem.GetSize();
        FString info = FString::Printf(TEXT("%d x %d"), size.X, size.Y);
        if (InItem.bTrimmed && InItem.bOpaqueBoundsValid)
        {
            info += FString::Printf(TEXT("  (trimmed from %d x %d)"), InItem.SourceSize.X, InItem.SourceSize.Y);
        }
        if (!FMath::IsNearlyEqual(InItem.Scale, 1.0f))
        {
            info += FString::Printf(TEXT("  %d%%"), FMath::RoundToInt(InItem.Scale * 100.0f));
        }
        return FText::FromString(info);
    }
}

void STextureAtlasPanel::Construct(const FArguments& InArgs)
{
    BaseAtlasBrush.DrawAs = ESlateBrushDrawType::Image;
    PreviousOptions = Options;

    OptionsView = FTextureToolkitWidgetUtils::MakeStructDetails(FTextureAtlasOptions::StaticStruct(), reinterpret_cast<uint8*>(&Options), OptionsScope);
    OptionsView->GetOnFinishedChangingPropertiesDelegate().AddSP(this, &STextureAtlasPanel::HandleOptionsChanged);

    ChildSlot
    [
        SNew(SSplitter)
        .Orientation(Orient_Horizontal)

        + SSplitter::Slot()
        .Value(0.3f)
        [
            SNew(SSplitter)
            .Orientation(Orient_Vertical)

            + SSplitter::Slot()
            .Value(0.45f)
            [
                SNew(SVerticalBox)

                + SVerticalBox::Slot()
                .AutoHeight()
                .Padding(4.0f)
                [
                    SNew(SHorizontalBox)

                    + SHorizontalBox::Slot()
                    .FillWidth(1.0f)
                    .VAlign(VAlign_Center)
                    [
                        SNew(STextBlock)
                        .Font(FAppStyle::GetFontStyle("NormalFontBold"))
                        .Text_Lambda([this]() { return FText::Format(LOCTEXT("SpritesHeader", "Sprites ({0})"), FText::AsNumber(Items.Num())); })
                    ]

                    + SHorizontalBox::Slot()
                    .AutoWidth()
                    .Padding(2.0f, 0.0f)
                    [
                        SNew(SButton)
                        .Text(LOCTEXT("RemoveSelected", "Remove Selected"))
                        .OnClicked(this, &STextureAtlasPanel::HandleRemoveSelectedClicked)
                    ]

                    + SHorizontalBox::Slot()
                    .AutoWidth()
                    [
                        SNew(SButton)
                        .Text(LOCTEXT("Clear", "Clear"))
                        .OnClicked(this, &STextureAtlasPanel::HandleClearClicked)
                    ]
                ]

                + SVerticalBox::Slot()
                .FillHeight(1.0f)
                .Padding(4.0f, 0.0f, 4.0f, 4.0f)
                [
                    SNew(SBorder)
                    .BorderImage(FAppStyle::GetBrush("Brushes.Recessed"))
                    .Padding(0.0f)
                    [
                        SAssignNew(ItemList, SListView<TSharedPtr<FTextureAtlasItem>>)
                        .ListItemsSource(&Items)
                        .SelectionMode(ESelectionMode::Multi)
                        .OnGenerateRow(this, &STextureAtlasPanel::HandleGenerateItemRow)
                        .OnSelectionChanged(this, &STextureAtlasPanel::HandleItemSelectionChanged)
                        .OnContextMenuOpening(this, &STextureAtlasPanel::HandleItemListContextMenu)
                    ]
                ]
            ]

            + SSplitter::Slot()
            .Value(0.2f)
            [
                SNew(SVerticalBox)

                + SVerticalBox::Slot()
                .AutoHeight()
                .Padding(4.0f)
                [
                    SNew(SHorizontalBox)

                    + SHorizontalBox::Slot()
                    .FillWidth(1.0f)
                    .VAlign(VAlign_Center)
                    [
                        SNew(STextBlock)
                        .Font(FAppStyle::GetFontStyle("NormalFontBold"))
                        .Text_Lambda([this]() { return FText::Format(LOCTEXT("WidgetsHeader", "Atlasify Widgets ({0})"), FText::AsNumber(AtlasifyWidgets.Num())); })
                        .ToolTipText(LOCTEXT("WidgetsTooltip", "Widget Blueprints in this list are updated after generation: every brush that uses a texture from the sprite list is switched to the generated PaperSprite."))
                    ]

                    + SHorizontalBox::Slot()
                    .AutoWidth()
                    .Padding(2.0f, 0.0f)
                    [
                        SNew(SButton)
                        .Text(LOCTEXT("RemoveWidgets", "Remove Selected"))
                        .OnClicked(this, &STextureAtlasPanel::HandleRemoveWidgetsClicked)
                    ]

                    + SHorizontalBox::Slot()
                    .AutoWidth()
                    [
                        SNew(SButton)
                        .Text(LOCTEXT("ClearWidgets", "Clear"))
                        .OnClicked(this, &STextureAtlasPanel::HandleClearWidgetsClicked)
                    ]
                ]

                + SVerticalBox::Slot()
                .FillHeight(1.0f)
                .Padding(4.0f, 0.0f, 4.0f, 4.0f)
                [
                    SNew(SBorder)
                    .BorderImage(FAppStyle::GetBrush("Brushes.Recessed"))
                    .Padding(0.0f)
                    [
                        SAssignNew(WidgetList, SListView<TSharedPtr<FAssetData>>)
                        .ListItemsSource(&AtlasifyWidgets)
                        .SelectionMode(ESelectionMode::Multi)
                        .OnGenerateRow(this, &STextureAtlasPanel::HandleGenerateWidgetRow)
                    ]
                ]
            ]

            + SSplitter::Slot()
            .Value(0.35f)
            [
                SNew(SVerticalBox)

                + SVerticalBox::Slot()
                .AutoHeight()
                .Padding(4.0f)
                [
                    FTextureToolkitWidgetUtils::MakeHeader(LOCTEXT("SettingsHeader", "Settings"))
                ]

                + SVerticalBox::Slot()
                .FillHeight(1.0f)
                [
                    OptionsView->GetWidget().ToSharedRef()
                ]
            ]
        ]

        + SSplitter::Slot()
        .Value(0.7f)
        [
            SNew(SVerticalBox)

            + SVerticalBox::Slot()
            .AutoHeight()
            .Padding(4.0f)
            [
                SNew(SHorizontalBox)

                + SHorizontalBox::Slot()
                .AutoWidth()
                .Padding(0.0f, 0.0f, 4.0f, 0.0f)
                [
                    SNew(SButton)
                    .Text(LOCTEXT("AutoArrange", "Auto Arrange"))
                    .ToolTipText(LOCTEXT("AutoArrangeTooltip", "Repacks every unlocked sprite and picks the smallest atlas size that fits (unless Manual Size is set)."))
                    .OnClicked(this, &STextureAtlasPanel::HandleAutoArrangeClicked)
                ]

                + SHorizontalBox::Slot()
                .AutoWidth()
                [
                    SNew(SButton)
                    .Text(LOCTEXT("Fit", "Fit View"))
                    .ToolTipText(LOCTEXT("FitTooltip", "Fits the atlas into the view (F)."))
                    .OnClicked(this, &STextureAtlasPanel::HandleFitClicked)
                ]

                + SHorizontalBox::Slot()
                .FillWidth(1.0f)
                .HAlign(HAlign_Right)
                .VAlign(VAlign_Center)
                [
                    SNew(STextBlock)
                    .Text(this, &STextureAtlasPanel::GetAtlasInfoText)
                ]
            ]

            + SVerticalBox::Slot()
            .FillHeight(1.0f)
            .Padding(4.0f, 0.0f)
            [
                SAssignNew(Canvas, STextureAtlasCanvas)
                .Items(&Items)
                .BaseOccupied(&BaseOccupied)
                .AtlasSize_Lambda([this]() { return AtlasSize; })
                .Padding_Lambda([this]() { return Options.Padding; })
                .BaseAtlasBrush_Lambda([this]() -> const FSlateBrush* { return BaseAtlas ? &BaseAtlasBrush : nullptr; })
                .BaseAtlasSize_Lambda([this]() { return BaseAtlasSize; })
                .OnItemSelected(this, &STextureAtlasPanel::HandleCanvasItemSelected)
                .OnItemsMoved(this, &STextureAtlasPanel::HandleCanvasItemsMoved)
                .OnItemContextMenu(this, &STextureAtlasPanel::HandleCanvasContextMenu)
                .OnDeleteRequested(this, &STextureAtlasPanel::HandleDeleteRequested)
            ]

            + SVerticalBox::Slot()
            .AutoHeight()
            .Padding(4.0f, 2.0f)
            [
                SNew(STextBlock)
                .Text(LOCTEXT("CanvasHint", "Drag to move (Shift snaps to 16 px), arrows nudge, wheel zooms, right or middle drag pans, right click for actions. Red outline: overlap or outside the atlas."))
                .ColorAndOpacity(FSlateColor::UseSubduedForeground())
                .AutoWrapText(true)
            ]

            + SVerticalBox::Slot()
            .AutoHeight()
            .Padding(4.0f, 2.0f)
            [
                BuildSelectedItemRow()
            ]

            + SVerticalBox::Slot()
            .AutoHeight()
            .Padding(4.0f, 2.0f)
            [
                BuildBaseAtlasRow()
            ]

            + SVerticalBox::Slot()
            .AutoHeight()
            .Padding(4.0f, 6.0f)
            [
                SNew(SHorizontalBox)

                + SHorizontalBox::Slot()
                .FillWidth(1.0f)
                .VAlign(VAlign_Center)
                [
                    SNew(STextBlock)
                    .Text_Lambda([this]() { return StatusText; })
                    .AutoWrapText(true)
                ]

                + SHorizontalBox::Slot()
                .AutoWidth()
                .Padding(4.0f, 0.0f)
                [
                    SNew(SButton)
                    .Text(LOCTEXT("ReplaceAll", "Replace All References"))
                    .ToolTipText(LOCTEXT("ReplaceAllTooltip", "Available after Generate. Loads every asset that references the source textures and switches FSlateBrush images to the generated PaperSprites. Plain Texture2D properties are skipped because a sprite cannot be assigned to them."))
                    .IsEnabled_Lambda([this]() { return GeneratedSprites.Num() > 0; })
                    .OnClicked(this, &STextureAtlasPanel::HandleReplaceReferencesClicked)
                ]

                + SHorizontalBox::Slot()
                .AutoWidth()
                [
                    SNew(SButton)
                    .ButtonStyle(FAppStyle::Get(), "PrimaryButton")
                    .Text(LOCTEXT("Generate", "Generate"))
                    .ToolTipText(LOCTEXT("GenerateTooltip", "Writes the atlas texture and the sprites. Existing assets with the same names are updated in place, so references stay valid."))
                    .IsEnabled_Lambda([this]() { return Items.Num() > 0; })
                    .OnClicked(this, &STextureAtlasPanel::HandleGenerateClicked)
                ]
            ]
        ]
    ];

    StatusText = LOCTEXT("InitialStatus", "Add textures from the browser (context menu, double click or drag and drop).");
}

STextureAtlasPanel::~STextureAtlasPanel() = default;

TSharedRef<SWidget> STextureAtlasPanel::BuildSelectedItemRow()
{
    return SNew(SHorizontalBox)

        + SHorizontalBox::Slot()
        .FillWidth(1.0f)
        .VAlign(VAlign_Center)
        [
            SNew(STextBlock)
            .Text_Lambda([this]()
            {
                const TSharedPtr<FTextureAtlasItem> item = GetSelectedItem();
                return item.IsValid()
                    ? FText::Format(LOCTEXT("SelectedItem", "Selected: {0}  {1}"), FText::FromString(GetNameSafe(item->Texture)), TextureAtlasPanelPrivate::GetItemInfo(*item))
                    : LOCTEXT("NoSelection", "Selected: none");
            })
        ]

        + SHorizontalBox::Slot()
        .AutoWidth()
        .VAlign(VAlign_Center)
        .Padding(8.0f, 0.0f, 4.0f, 0.0f)
        [
            SNew(STextBlock)
            .Text(INVTEXT("X"))
        ]

        + SHorizontalBox::Slot()
        .AutoWidth()
        [
            SNew(SBox)
            .WidthOverride(70.0f)
            [
                SNew(SNumericEntryBox<int32>)
                .IsEnabled_Lambda([this]() { return GetSelectedItem().IsValid(); })
                .Value_Lambda([this]() -> TOptional<int32>
                {
                    const TSharedPtr<FTextureAtlasItem> item = GetSelectedItem();
                    return item.IsValid() ? TOptional<int32>(item->Position.X) : TOptional<int32>();
                })
                .OnValueCommitted_Lambda([this](int32 InValue, ETextCommit::Type InCommitType)
                {
                    if (const TSharedPtr<FTextureAtlasItem> item = GetSelectedItem())
                    {
                        item->Position.X = FMath::Max(0, InValue);
                        HandleCanvasItemsMoved();
                    }
                })
            ]
        ]

        + SHorizontalBox::Slot()
        .AutoWidth()
        .VAlign(VAlign_Center)
        .Padding(8.0f, 0.0f, 4.0f, 0.0f)
        [
            SNew(STextBlock)
            .Text(INVTEXT("Y"))
        ]

        + SHorizontalBox::Slot()
        .AutoWidth()
        [
            SNew(SBox)
            .WidthOverride(70.0f)
            [
                SNew(SNumericEntryBox<int32>)
                .IsEnabled_Lambda([this]() { return GetSelectedItem().IsValid(); })
                .Value_Lambda([this]() -> TOptional<int32>
                {
                    const TSharedPtr<FTextureAtlasItem> item = GetSelectedItem();
                    return item.IsValid() ? TOptional<int32>(item->Position.Y) : TOptional<int32>();
                })
                .OnValueCommitted_Lambda([this](int32 InValue, ETextCommit::Type InCommitType)
                {
                    if (const TSharedPtr<FTextureAtlasItem> item = GetSelectedItem())
                    {
                        item->Position.Y = FMath::Max(0, InValue);
                        HandleCanvasItemsMoved();
                    }
                })
            ]
        ]

        + SHorizontalBox::Slot()
        .AutoWidth()
        .VAlign(VAlign_Center)
        .Padding(8.0f, 0.0f, 4.0f, 0.0f)
        [
            SNew(STextBlock)
            .Text(LOCTEXT("ScalePercent", "Scale %"))
        ]

        + SHorizontalBox::Slot()
        .AutoWidth()
        [
            SNew(SBox)
            .WidthOverride(70.0f)
            [
                SNew(SSpinBox<float>)
                .MinValue(5.0f)
                .MaxValue(400.0f)
                .MinSliderValue(5.0f)
                .MaxSliderValue(200.0f)
                .Delta(5.0f)
                .IsEnabled_Lambda([this]() { return GetSelectedItem().IsValid(); })
                .Value_Lambda([this]()
                {
                    const TSharedPtr<FTextureAtlasItem> item = GetSelectedItem();
                    return item.IsValid() ? item->Scale * 100.0f : 100.0f;
                })
                .OnValueCommitted_Lambda([this](float InValue, ETextCommit::Type InCommitType)
                {
                    if (const TSharedPtr<FTextureAtlasItem> item = GetSelectedItem())
                    {
                        SetItemsScale({ item }, InValue / 100.0f);
                    }
                })
            ]
        ];
}

TSharedRef<SWidget> STextureAtlasPanel::BuildBaseAtlasRow()
{
    return SNew(SHorizontalBox)

        + SHorizontalBox::Slot()
        .AutoWidth()
        .VAlign(VAlign_Center)
        .Padding(0.0f, 0.0f, 8.0f, 0.0f)
        [
            SNew(STextBlock)
            .Text(LOCTEXT("BaseAtlas", "Base Atlas"))
            .ToolTipText(LOCTEXT("BaseAtlasTooltip", "Existing atlas to extend. Its pixels and sprites stay in place, new sprites go into free space. The output name is set to the base atlas so it is updated in place."))
        ]

        + SHorizontalBox::Slot()
        .FillWidth(0.45f)
        .VAlign(VAlign_Center)
        [
            SNew(SObjectPropertyEntryBox)
            .AllowedClass(UTexture2D::StaticClass())
            .ObjectPath_Lambda([this]() { return BaseAtlas ? BaseAtlas->GetPathName() : FString(); })
            .OnObjectChanged_Lambda([this](const FAssetData& InAssetData) { SetBaseAtlas(Cast<UTexture2D>(InAssetData.GetAsset())); })
            .ThumbnailPool(UThumbnailManager::Get().GetSharedThumbnailPool())
            .DisplayThumbnail(false)
            .AllowClear(true)
        ]

        + SHorizontalBox::Slot()
        .FillWidth(0.55f)
        .VAlign(VAlign_Center)
        .Padding(8.0f, 0.0f, 0.0f, 0.0f)
        [
            SNew(STextBlock)
            .Text(this, &STextureAtlasPanel::GetBaseAtlasText)
            .ColorAndOpacity(FSlateColor::UseSubduedForeground())
            .AutoWrapText(true)
        ];
}

TSharedRef<ITableRow> STextureAtlasPanel::HandleGenerateItemRow(TSharedPtr<FTextureAtlasItem> InItem, const TSharedRef<STableViewBase>& InOwnerTable)
{
    using namespace TextureAtlasPanelPrivate;

    const TSharedPtr<FTextureAtlasItem> item = InItem;
    return SNew(STableRow<TSharedPtr<FTextureAtlasItem>>, InOwnerTable)
        .Padding(FMargin(2.0f))
        [
            SNew(SHorizontalBox)

            + SHorizontalBox::Slot()
            .AutoWidth()
            .VAlign(VAlign_Center)
            .Padding(0.0f, 0.0f, 6.0f, 0.0f)
            [
                SNew(SBox)
                .WidthOverride(32.0f)
                .HeightOverride(32.0f)
                [
                    SNew(SScaleBox)
                    .Stretch(EStretch::ScaleToFit)
                    [
                        SNew(SImage)
                        .Image_Lambda([item]() -> const FSlateBrush* { return &item->Brush; })
                    ]
                ]
            ]

            + SHorizontalBox::Slot()
            .FillWidth(1.0f)
            .VAlign(VAlign_Center)
            [
                SNew(SVerticalBox)

                + SVerticalBox::Slot()
                .AutoHeight()
                [
                    SNew(STextBlock)
                    .Text(FText::FromString(GetNameSafe(item->Texture)))
                    .ToolTipText(FText::FromString(item->Texture ? item->Texture->GetPathName() : FString()))
                ]

                + SVerticalBox::Slot()
                .AutoHeight()
                [
                    SNew(STextBlock)
                    .Text_Lambda([item]() { return GetItemInfo(*item); })
                    .ColorAndOpacity(FSlateColor::UseSubduedForeground())
                ]
            ]

            + SHorizontalBox::Slot()
            .AutoWidth()
            .VAlign(VAlign_Center)
            [
                SNew(SButton)
                .ButtonStyle(FAppStyle::Get(), "SimpleButton")
                .ToolTipText(LOCTEXT("LockTooltip", "Locked sprites keep their position on Auto Arrange."))
                .OnClicked_Lambda([item]()
                {
                    item->bLocked = !item->bLocked;
                    return FReply::Handled();
                })
                [
                    SNew(SImage)
                    .Image_Lambda([item]() { return FAppStyle::GetBrush(item->bLocked ? "Icons.Lock" : "Icons.Unlock"); })
                    .ColorAndOpacity(FSlateColor::UseForeground())
                ]
            ]

            + SHorizontalBox::Slot()
            .AutoWidth()
            .VAlign(VAlign_Center)
            [
                MakeIconButton("Icons.Search", LOCTEXT("LocateTooltip", "Show in Content Browser"), FOnClicked::CreateLambda([item]()
                {
                    FTextureToolkitAssetUtils::BrowseToAssets({ item->Texture.Get() });
                    return FReply::Handled();
                }))
            ]

            + SHorizontalBox::Slot()
            .AutoWidth()
            .VAlign(VAlign_Center)
            [
                MakeIconButton("Icons.Delete", LOCTEXT("RemoveTooltip", "Remove from the atlas"), FOnClicked::CreateSPLambda(this, [this, item]()
                {
                    RemoveItems({ item });
                    return FReply::Handled();
                }))
            ]
        ];
}

TSharedRef<ITableRow> STextureAtlasPanel::HandleGenerateWidgetRow(TSharedPtr<FAssetData> InAsset, const TSharedRef<STableViewBase>& InOwnerTable)
{
    return SNew(STableRow<TSharedPtr<FAssetData>>, InOwnerTable)
        .Padding(FMargin(4.0f, 2.0f))
        [
            SNew(STextBlock)
            .Text(FText::FromName(InAsset->AssetName))
            .ToolTipText(FText::FromString(InAsset->GetObjectPathString()))
        ];
}

void STextureAtlasPanel::HandleItemSelectionChanged(TSharedPtr<FTextureAtlasItem> InItem, ESelectInfo::Type InSelectInfo)
{
    if (InSelectInfo != ESelectInfo::Direct && Canvas.IsValid())
    {
        Canvas->SetSelectedItem(InItem);
    }
}

void STextureAtlasPanel::HandleCanvasItemSelected(TSharedPtr<FTextureAtlasItem> InItem)
{
    if (!ItemList.IsValid())
    {
        return;
    }

    ItemList->ClearSelection();
    if (InItem.IsValid())
    {
        ItemList->SetItemSelection(InItem, true, ESelectInfo::Direct);
        ItemList->RequestScrollIntoView(InItem);
    }
}

void STextureAtlasPanel::HandleCanvasItemsMoved()
{
    TSet<const FTextureAtlasItem*> invalidItems;
    STextureAtlasCanvas::FindInvalidItems(Items, BaseOccupied, AtlasSize, Options.Padding, invalidItems);
    StatusText = invalidItems.Num() > 0
        ? FText::Format(LOCTEXT("InvalidAfterMove", "{0} sprite(s) overlap or are outside the atlas."), FText::AsNumber(invalidItems.Num()))
        : FText::GetEmpty();
}

TSharedPtr<SWidget> STextureAtlasPanel::HandleCanvasContextMenu(TSharedPtr<FTextureAtlasItem> InItem)
{
    FMenuBuilder menuBuilder(true, nullptr);

    if (InItem.IsValid())
    {
        FillItemMenu(menuBuilder, { InItem });
    }

    menuBuilder.BeginSection(TEXT("Atlas"), LOCTEXT("AtlasSection", "Atlas"));
    menuBuilder.AddMenuEntry(
        LOCTEXT("MenuAutoArrange", "Auto Arrange"),
        FText::GetEmpty(),
        FSlateIcon(),
        FUIAction(FExecuteAction::CreateLambda([this]() { AutoArrange(); })));
    menuBuilder.AddMenuEntry(
        LOCTEXT("MenuFit", "Fit View"),
        FText::GetEmpty(),
        FSlateIcon(),
        FUIAction(FExecuteAction::CreateLambda([this]() { Canvas->RequestFit(); })));
    menuBuilder.EndSection();

    return menuBuilder.MakeWidget();
}

TSharedPtr<SWidget> STextureAtlasPanel::HandleItemListContextMenu()
{
    const TArray<TSharedPtr<FTextureAtlasItem>> selectedItems = GetSelectedItems();
    if (selectedItems.Num() == 0)
    {
        return nullptr;
    }

    FMenuBuilder menuBuilder(true, nullptr);
    FillItemMenu(menuBuilder, selectedItems);
    return menuBuilder.MakeWidget();
}

void STextureAtlasPanel::FillItemMenu(FMenuBuilder& InMenuBuilder, const TArray<TSharedPtr<FTextureAtlasItem>>& InItems)
{
    if (InItems.Num() == 0)
    {
        return;
    }

    const bool bAllLocked = !InItems.ContainsByPredicate([](const TSharedPtr<FTextureAtlasItem>& InItem) { return !InItem->bLocked; });

    InMenuBuilder.BeginSection(TEXT("Sprite"), LOCTEXT("SpriteSection", "Sprite"));

    InMenuBuilder.AddMenuEntry(
        bAllLocked ? LOCTEXT("Unlock", "Unlock Position") : LOCTEXT("Lock", "Lock Position"),
        LOCTEXT("LockMenuTooltip", "Locked sprites keep their position on Auto Arrange."),
        FSlateIcon(FAppStyle::GetAppStyleSetName(), bAllLocked ? "Icons.Unlock" : "Icons.Lock"),
        FUIAction(FExecuteAction::CreateLambda([this, InItems, bAllLocked]() { SetItemsLocked(InItems, !bAllLocked); })));

    InMenuBuilder.AddSubMenu(
        LOCTEXT("Scale", "Scale"),
        LOCTEXT("ScaleTooltip", "Scales the sprite in the atlas. Run Auto Arrange afterwards to repack."),
        FNewMenuDelegate::CreateLambda([this, InItems](FMenuBuilder& InSubMenuBuilder)
        {
            for (const float scale : { 0.25f, 0.5f, 0.75f, 1.0f, 1.5f, 2.0f })
            {
                InSubMenuBuilder.AddMenuEntry(
                    FText::Format(LOCTEXT("ScaleEntry", "{0}%"), FText::AsNumber(FMath::RoundToInt(scale * 100.0f))),
                    FText::GetEmpty(),
                    FSlateIcon(),
                    FUIAction(FExecuteAction::CreateLambda([this, InItems, scale]() { SetItemsScale(InItems, scale); })));
            }
        }));

    InMenuBuilder.AddMenuEntry(
        LOCTEXT("ShowInBrowser", "Show in Content Browser"),
        FText::GetEmpty(),
        FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.Search"),
        FUIAction(FExecuteAction::CreateLambda([InItems]()
        {
            TArray<UObject*> textures;
            for (const TSharedPtr<FTextureAtlasItem>& item : InItems)
            {
                textures.Add(item->Texture);
            }
            FTextureToolkitAssetUtils::BrowseToAssets(textures);
        })));

    InMenuBuilder.AddMenuEntry(
        LOCTEXT("RemoveMenu", "Remove"),
        FText::GetEmpty(),
        FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.Delete"),
        FUIAction(FExecuteAction::CreateLambda([this, InItems]() { RemoveItems(InItems); })));

    InMenuBuilder.EndSection();
}

void STextureAtlasPanel::HandleDeleteRequested(TSharedPtr<FTextureAtlasItem> InItem)
{
    if (InItem.IsValid())
    {
        RemoveItems({ InItem });
    }
}

void STextureAtlasPanel::HandleOptionsChanged(const FPropertyChangedEvent& InEvent)
{
    const bool bTrimChanged = Options.bTrimTransparentBorders != PreviousOptions.bTrimTransparentBorders;
    const bool bThresholdChanged = Options.AlphaThreshold != PreviousOptions.AlphaThreshold;
    const bool bLayoutChanged = Options.Padding != PreviousOptions.Padding
        || Options.bManualSize != PreviousOptions.bManualSize
        || Options.ManualWidth != PreviousOptions.ManualWidth
        || Options.ManualHeight != PreviousOptions.ManualHeight
        || Options.MaxAtlasSize != PreviousOptions.MaxAtlasSize
        || Options.bAllowNonSquare != PreviousOptions.bAllowNonSquare;

    PreviousOptions = Options;

    if (bThresholdChanged && BaseAtlas)
    {
        AnalyzeBaseAtlas();
    }
    if (bTrimChanged || bThresholdChanged)
    {
        ApplyTrimToItems(bThresholdChanged);
    }
    if (bTrimChanged || bThresholdChanged || bLayoutChanged)
    {
        AutoArrange();
    }
}

FReply STextureAtlasPanel::HandleAutoArrangeClicked()
{
    AutoArrange();
    return FReply::Handled();
}

FReply STextureAtlasPanel::HandleFitClicked()
{
    if (Canvas.IsValid())
    {
        Canvas->RequestFit();
    }
    return FReply::Handled();
}

FReply STextureAtlasPanel::HandleRemoveSelectedClicked()
{
    RemoveItems(GetSelectedItems());
    return FReply::Handled();
}

FReply STextureAtlasPanel::HandleClearClicked()
{
    RemoveItems(Items);
    return FReply::Handled();
}

FReply STextureAtlasPanel::HandleClearBaseClicked()
{
    SetBaseAtlas(nullptr);
    return FReply::Handled();
}

FReply STextureAtlasPanel::HandleRemoveWidgetsClicked()
{
    if (WidgetList.IsValid())
    {
        const TArray<TSharedPtr<FAssetData>> selectedWidgets = WidgetList->GetSelectedItems();
        AtlasifyWidgets.RemoveAll([&selectedWidgets](const TSharedPtr<FAssetData>& InAsset) { return selectedWidgets.Contains(InAsset); });
        WidgetList->ClearSelection();
    }
    RefreshLists();
    return FReply::Handled();
}

FReply STextureAtlasPanel::HandleClearWidgetsClicked()
{
    AtlasifyWidgets.Reset();
    RefreshLists();
    return FReply::Handled();
}

FReply STextureAtlasPanel::HandleGenerateClicked()
{
    if (Items.Num() == 0)
    {
        return FReply::Handled();
    }

    TSet<const FTextureAtlasItem*> invalidItems;
    STextureAtlasCanvas::FindInvalidItems(Items, BaseOccupied, AtlasSize, Options.Padding, invalidItems);
    if (invalidItems.Num() > 0)
    {
        const FText message = FText::Format(
            LOCTEXT("ConfirmInvalid", "{0} sprite(s) overlap each other, the base atlas content, or leave the atlas. Overlapping pixels will be overwritten.\n\nGenerate anyway?"),
            FText::AsNumber(invalidItems.Num()));
        if (FMessageDialog::Open(EAppMsgType::YesNo, message, LOCTEXT("ConfirmInvalidTitle", "Atlas Layout")) != EAppReturnType::Yes)
        {
            return FReply::Handled();
        }
    }

    FTextureAtlasBuildInput buildInput;
    buildInput.AtlasSize = AtlasSize;
    buildInput.Options = Options;
    buildInput.Items = Items;
    buildInput.BaseAtlas = BaseAtlas;
    buildInput.BaseAtlasSize = BaseAtlasSize;
    for (UPaperSprite* sprite : BaseSprites)
    {
        buildInput.BaseSprites.Add(sprite);
    }

    FTextureAtlasBuildResult buildResult;
    FText error;
    if (!FTextureAtlasBuilder::Build(buildInput, buildResult, error))
    {
        StatusText = error;
        FTextureToolkitAssetUtils::Notify(error, false);
        return FReply::Handled();
    }

    GeneratedAtlas = buildResult.Atlas;
    GeneratedSprites.Reset();
    for (const TPair<UTexture2D*, UPaperSprite*>& pair : buildResult.Sprites)
    {
        GeneratedSprites.Add(pair.Key, pair.Value);
    }

    if (BaseAtlas && GeneratedAtlas == BaseAtlas)
    {
        BaseAtlasSize = AtlasSize;
        BaseAtlasBrush.ImageSize = FVector2D(AtlasSize.X, AtlasSize.Y);
    }

    StatusText = FText::Format(LOCTEXT("Generated", "Generated {0} ({1} x {2}) with {3} sprite(s)."),
        FText::FromString(GetNameSafe(GeneratedAtlas)), FText::AsNumber(AtlasSize.X), FText::AsNumber(AtlasSize.Y), FText::AsNumber(GeneratedSprites.Num()));
    FTextureToolkitAssetUtils::Notify(StatusText, true);

    if (AtlasifyWidgets.Num() > 0 && GeneratedSprites.Num() > 0)
    {
        ReplaceInAtlasifyWidgets();
    }

    FTextureToolkitAssetUtils::BrowseToAssets({ GeneratedAtlas.Get() });
    return FReply::Handled();
}

FReply STextureAtlasPanel::HandleReplaceReferencesClicked()
{
    const TMap<UObject*, UObject*> replacements = BuildReplacementMap();
    if (replacements.Num() == 0)
    {
        return FReply::Handled();
    }

    const FText message = FText::Format(
        LOCTEXT("ConfirmReplace", "Replace references to {0} texture(s) with the generated sprites in every asset that uses them?\n\nReferencing assets are loaded, their FSlateBrush images are switched to the sprites and blueprints are recompiled. Texture2D-only properties (materials and similar) are left unchanged."),
        FText::AsNumber(replacements.Num()));
    if (FMessageDialog::Open(EAppMsgType::YesNo, message, LOCTEXT("ConfirmReplaceTitle", "Replace All References")) != EAppReturnType::Yes)
    {
        return FReply::Handled();
    }

    FTextureReplaceReport report;
    FTextureToolkitAssetUtils::ReplaceBrushResources(replacements, TArray<FName>(), GetProtectedPackages(), Options.bSaveAssets, report);

    StatusText = FText::Format(LOCTEXT("Replaced", "Replaced {0} brush(es) in {1} of {2} scanned asset(s), recompiled {3} blueprint(s)."),
        FText::AsNumber(report.BrushesReplaced), FText::AsNumber(report.PackagesModified), FText::AsNumber(report.PackagesScanned), FText::AsNumber(report.BlueprintsCompiled));
    FTextureToolkitAssetUtils::Notify(StatusText, true);
    return FReply::Handled();
}

bool STextureAtlasPanel::AutoArrange()
{
    FTextureAtlasPackParams params;
    params.Padding = Options.Padding;
    params.MaxSize = TextureToolkitSizeToPixels(Options.MaxAtlasSize);
    params.bAllowNonSquare = Options.bAllowNonSquare;
    params.Obstacles = BaseOccupied;
    params.MinSize = BaseAtlas ? BaseAtlasSize : FIntPoint::ZeroValue;

    TArray<TSharedPtr<FTextureAtlasItem>> movableItems;
    TArray<FIntPoint> sizes;
    for (const TSharedPtr<FTextureAtlasItem>& item : Items)
    {
        if (item->bLocked)
        {
            const FIntRect lockedRect = item->GetRect();
            params.Obstacles.Add(lockedRect);
            params.MinSize = params.MinSize.ComponentMax(FIntPoint(
                static_cast<int32>(FMath::RoundUpToPowerOfTwo(static_cast<uint32>(FMath::Max(1, lockedRect.Max.X)))),
                static_cast<int32>(FMath::RoundUpToPowerOfTwo(static_cast<uint32>(FMath::Max(1, lockedRect.Max.Y))))));
        }
        else
        {
            movableItems.Add(item);
            sizes.Add(item->GetSize());
        }
    }

    if (Options.bManualSize)
    {
        params.FixedSize = FIntPoint(TextureToolkitSizeToPixels(Options.ManualWidth), TextureToolkitSizeToPixels(Options.ManualHeight));
        if (BaseAtlas)
        {
            params.FixedSize = params.FixedSize.ComponentMax(BaseAtlasSize);
        }
    }

    FIntPoint resultSize = FIntPoint::ZeroValue;
    TArray<FIntPoint> positions;
    if (!FTextureAtlasPacker::Pack(params, sizes, resultSize, positions))
    {
        if (Options.bManualSize)
        {
            AtlasSize = params.FixedSize;
        }
        StatusText = LOCTEXT("PackFailed", "Sprites do not fit. Increase Max Atlas Size or the manual size, reduce sprite scale or padding.");
        FTextureToolkitAssetUtils::Notify(StatusText, false);
        return false;
    }

    AtlasSize = resultSize;
    for (int32 index = 0; index < movableItems.Num(); ++index)
    {
        movableItems[index]->Position = positions[index];
    }

    StatusText = FText::Format(LOCTEXT("Arranged", "Arranged {0} sprite(s) into {1} x {2}."),
        FText::AsNumber(movableItems.Num()), FText::AsNumber(AtlasSize.X), FText::AsNumber(AtlasSize.Y));
    return true;
}

void STextureAtlasPanel::PlaceNewItems(const TArray<TSharedPtr<FTextureAtlasItem>>& InNewItems)
{
    if (InNewItems.Num() == Items.Num())
    {
        AutoArrange();
        return;
    }

    FTextureAtlasPackParams params;
    params.Padding = Options.Padding;
    params.MaxSize = TextureToolkitSizeToPixels(Options.MaxAtlasSize);
    params.bAllowNonSquare = Options.bAllowNonSquare;
    params.Obstacles = BaseOccupied;

    for (const TSharedPtr<FTextureAtlasItem>& item : Items)
    {
        if (!InNewItems.Contains(item))
        {
            params.Obstacles.Add(item->GetRect());
        }
    }

    if (Options.bManualSize)
    {
        params.FixedSize = AtlasSize;
    }
    else
    {
        params.MinSize = AtlasSize;
    }

    TArray<FIntPoint> sizes;
    for (const TSharedPtr<FTextureAtlasItem>& item : InNewItems)
    {
        sizes.Add(item->GetSize());
    }

    FIntPoint resultSize = FIntPoint::ZeroValue;
    TArray<FIntPoint> positions;
    if (!FTextureAtlasPacker::Pack(params, sizes, resultSize, positions))
    {
        AutoArrange();
        return;
    }

    AtlasSize = resultSize;
    for (int32 index = 0; index < InNewItems.Num(); ++index)
    {
        InNewItems[index]->Position = positions[index];
    }

    StatusText = FText::Format(LOCTEXT("Placed", "Placed {0} new sprite(s), atlas {1} x {2}."),
        FText::AsNumber(InNewItems.Num()), FText::AsNumber(AtlasSize.X), FText::AsNumber(AtlasSize.Y));
}

void STextureAtlasPanel::ApplyTrimToItems(bool bInRecomputeBounds)
{
    const bool bTrim = Options.bTrimTransparentBorders;
    const uint8 alphaThreshold = static_cast<uint8>(FMath::Clamp(Options.AlphaThreshold, 0, 254));

    int32 boundsToCompute = 0;
    for (const TSharedPtr<FTextureAtlasItem>& item : Items)
    {
        if (bInRecomputeBounds)
        {
            item->bOpaqueBoundsValid = false;
        }
        if (bTrim && !item->bOpaqueBoundsValid)
        {
            ++boundsToCompute;
        }
    }

    FScopedSlowTask slowTask(static_cast<float>(boundsToCompute), LOCTEXT("ComputingBounds", "Computing opaque bounds..."));
    if (boundsToCompute > 4)
    {
        slowTask.MakeDialog();
    }

    for (const TSharedPtr<FTextureAtlasItem>& item : Items)
    {
        item->bTrimmed = bTrim;
        if (bTrim && !item->bOpaqueBoundsValid)
        {
            slowTask.EnterProgressFrame(1.0f, FText::FromString(GetNameSafe(item->Texture)));
            FTextureAtlasBuilder::ComputeOpaqueBounds(*item, alphaThreshold);
        }
        item->RefreshBrush();
    }
}

void STextureAtlasPanel::AnalyzeBaseAtlas()
{
    BaseOccupied.Reset();
    BaseSprites.Reset();
    if (!BaseAtlas)
    {
        return;
    }

    TArray<UPaperSprite*> sprites;
    FTextureAtlasBuilder::AnalyzeBaseAtlas(BaseAtlas, static_cast<uint8>(FMath::Clamp(Options.AlphaThreshold, 0, 254)), BaseOccupied, sprites);
    for (UPaperSprite* sprite : sprites)
    {
        BaseSprites.Add(sprite);
    }
}

void STextureAtlasPanel::RemoveItems(const TArray<TSharedPtr<FTextureAtlasItem>>& InItems)
{
    if (InItems.Num() == 0)
    {
        return;
    }

    const TArray<TSharedPtr<FTextureAtlasItem>> itemsToRemove = InItems;
    Items.RemoveAll([&itemsToRemove](const TSharedPtr<FTextureAtlasItem>& InItem) { return itemsToRemove.Contains(InItem); });

    if (Canvas.IsValid() && itemsToRemove.Contains(Canvas->GetSelectedItem()))
    {
        Canvas->SetSelectedItem(nullptr);
    }
    if (ItemList.IsValid())
    {
        ItemList->ClearSelection();
    }
    RefreshLists();
}

void STextureAtlasPanel::RefreshLists()
{
    if (ItemList.IsValid())
    {
        ItemList->RequestListRefresh();
    }
    if (WidgetList.IsValid())
    {
        WidgetList->RequestListRefresh();
    }
}

void STextureAtlasPanel::RefreshOptionsView()
{
    PreviousOptions = Options;
    if (OptionsView.IsValid() && OptionsView->GetDetailsView())
    {
        OptionsView->GetDetailsView()->ForceRefresh();
    }
}

void STextureAtlasPanel::SetItemsScale(const TArray<TSharedPtr<FTextureAtlasItem>>& InItems, float InScale)
{
    const float scale = FMath::Clamp(InScale, 0.01f, 8.0f);
    for (const TSharedPtr<FTextureAtlasItem>& item : InItems)
    {
        item->Scale = scale;
    }
    HandleCanvasItemsMoved();
}

void STextureAtlasPanel::SetItemsLocked(const TArray<TSharedPtr<FTextureAtlasItem>>& InItems, bool bInLocked)
{
    for (const TSharedPtr<FTextureAtlasItem>& item : InItems)
    {
        item->bLocked = bInLocked;
    }
}

TArray<TSharedPtr<FTextureAtlasItem>> STextureAtlasPanel::GetSelectedItems() const
{
    TArray<TSharedPtr<FTextureAtlasItem>> selectedItems;
    if (ItemList.IsValid())
    {
        selectedItems = ItemList->GetSelectedItems();
    }
    if (selectedItems.Num() == 0 && Canvas.IsValid() && Canvas->GetSelectedItem().IsValid())
    {
        selectedItems.Add(Canvas->GetSelectedItem());
    }
    return selectedItems;
}

TSharedPtr<FTextureAtlasItem> STextureAtlasPanel::GetSelectedItem() const
{
    return Canvas.IsValid() ? Canvas->GetSelectedItem() : nullptr;
}

TSharedPtr<FTextureAtlasItem> STextureAtlasPanel::FindItem(const UTexture2D* InTexture) const
{
    const TSharedPtr<FTextureAtlasItem>* found = Items.FindByPredicate([InTexture](const TSharedPtr<FTextureAtlasItem>& InItem)
    {
        return InItem->Texture == InTexture;
    });
    return found ? *found : nullptr;
}

TMap<UObject*, UObject*> STextureAtlasPanel::BuildReplacementMap() const
{
    TMap<UObject*, UObject*> replacements;
    for (const TPair<TObjectPtr<UTexture2D>, TObjectPtr<UPaperSprite>>& pair : GeneratedSprites)
    {
        if (pair.Key && pair.Value)
        {
            replacements.Add(pair.Key.Get(), pair.Value.Get());
        }
    }
    return replacements;
}

TSet<FName> STextureAtlasPanel::GetProtectedPackages() const
{
    TSet<FName> packages;
    if (GeneratedAtlas)
    {
        packages.Add(GeneratedAtlas->GetPackage()->GetFName());
    }
    for (const TPair<TObjectPtr<UTexture2D>, TObjectPtr<UPaperSprite>>& pair : GeneratedSprites)
    {
        if (pair.Key)
        {
            packages.Add(pair.Key->GetPackage()->GetFName());
        }
        if (pair.Value)
        {
            packages.Add(pair.Value->GetPackage()->GetFName());
        }
    }
    return packages;
}

void STextureAtlasPanel::ReplaceInAtlasifyWidgets()
{
    TArray<FName> packages;
    for (const TSharedPtr<FAssetData>& widgetAsset : AtlasifyWidgets)
    {
        packages.AddUnique(widgetAsset->PackageName);
    }

    FTextureReplaceReport report;
    FTextureToolkitAssetUtils::ReplaceBrushResources(BuildReplacementMap(), packages, GetProtectedPackages(), Options.bSaveAssets, report);

    FTextureToolkitAssetUtils::Notify(FText::Format(LOCTEXT("Atlasified", "Atlasify: replaced {0} brush(es) in {1} widget(s)."),
        FText::AsNumber(report.BrushesReplaced), FText::AsNumber(report.PackagesModified)), true);
}

FText STextureAtlasPanel::GetAtlasInfoText() const
{
    const int64 atlasArea = static_cast<int64>(AtlasSize.X) * AtlasSize.Y;
    int64 usedArea = 0;
    for (const TSharedPtr<FTextureAtlasItem>& item : Items)
    {
        const FIntPoint size = item->GetSize();
        usedArea += static_cast<int64>(size.X) * size.Y;
    }
    for (const FIntRect& occupied : BaseOccupied)
    {
        usedArea += static_cast<int64>(occupied.Width()) * occupied.Height();
    }

    const int32 usedPercent = atlasArea > 0 ? FMath::Min(100, static_cast<int32>(usedArea * 100 / atlasArea)) : 0;
    const EPixelFormat pixelFormat = FTextureToolkitImageUtils::GuessPixelFormat(Options.CompressionSettings, true);
    const int64 memory = FTextureToolkitImageUtils::EstimateMemoryBytes(AtlasSize, pixelFormat, Options.MipGenSettings != TMGS_NoMipmaps);
    const int32 zoomPercent = Canvas.IsValid() ? FMath::RoundToInt(Canvas->GetZoom() * 100.0f) : 100;

    return FText::Format(LOCTEXT("AtlasInfo", "{0} x {1}   |   {2} sprite(s)   |   ~{3}% used   |   ~{4} in memory   |   zoom {5}%"),
        FText::AsNumber(AtlasSize.X), FText::AsNumber(AtlasSize.Y), FText::AsNumber(Items.Num()), FText::AsNumber(usedPercent),
        FText::FromString(FTextureToolkitImageUtils::FormatBytes(memory)), FText::AsNumber(zoomPercent));
}

FText STextureAtlasPanel::GetBaseAtlasText() const
{
    if (!BaseAtlas)
    {
        return LOCTEXT("NoBaseAtlas", "Optional: pick an existing atlas to add new sprites into its free space.");
    }
    return FText::Format(LOCTEXT("BaseAtlasInfo", "{0} x {1}, {2} existing sprite(s), {3} occupied region(s)"),
        FText::AsNumber(BaseAtlasSize.X), FText::AsNumber(BaseAtlasSize.Y), FText::AsNumber(BaseSprites.Num()), FText::AsNumber(BaseOccupied.Num()));
}

void STextureAtlasPanel::AddTextures(const TArray<UTexture2D*>& InTextures)
{
    const uint8 alphaThreshold = static_cast<uint8>(FMath::Clamp(Options.AlphaThreshold, 0, 254));

    FScopedSlowTask slowTask(static_cast<float>(InTextures.Num()), LOCTEXT("AddingTextures", "Adding textures to the atlas..."));
    if (Options.bTrimTransparentBorders && InTextures.Num() > 4)
    {
        slowTask.MakeDialog(true);
    }

    TArray<TSharedPtr<FTextureAtlasItem>> newItems;
    int32 skippedCount = 0;
    for (UTexture2D* texture : InTextures)
    {
        slowTask.EnterProgressFrame(1.0f);
        if (!texture || texture == BaseAtlas || FindItem(texture).IsValid())
        {
            continue;
        }
        if (!FTextureToolkitImageUtils::CanReadSource(texture))
        {
            ++skippedCount;
            continue;
        }

        TSharedPtr<FTextureAtlasItem> item = MakeShared<FTextureAtlasItem>();
        item->Texture = texture;
        item->SourceSize = FIntPoint(static_cast<int32>(texture->Source.GetSizeX()), static_cast<int32>(texture->Source.GetSizeY()));
        item->bTrimmed = Options.bTrimTransparentBorders;
        if (item->bTrimmed)
        {
            FTextureAtlasBuilder::ComputeOpaqueBounds(*item, alphaThreshold);
        }
        item->RefreshBrush();
        newItems.Add(item);
    }

    if (skippedCount > 0)
    {
        FTextureToolkitAssetUtils::Notify(FText::Format(LOCTEXT("SkippedTextures", "Skipped {0} texture(s) without readable source data (UDIM, layered or missing source)."), FText::AsNumber(skippedCount)), false);
    }
    if (newItems.Num() == 0)
    {
        return;
    }

    Items.Append(newItems);
    PlaceNewItems(newItems);
    RefreshLists();
}

void STextureAtlasPanel::SetBaseAtlas(UTexture2D* InTexture)
{
    if (InTexture && !FTextureToolkitImageUtils::CanReadSource(InTexture))
    {
        FTextureToolkitAssetUtils::Notify(LOCTEXT("BaseUnreadable", "The base atlas has no readable source data."), false);
        return;
    }

    BaseAtlas = InTexture;
    BaseAtlasBrush.SetResourceObject(InTexture);

    if (!InTexture)
    {
        BaseAtlasSize = FIntPoint::ZeroValue;
        BaseOccupied.Reset();
        BaseSprites.Reset();
        AutoArrange();
        return;
    }

    BaseAtlasSize = FIntPoint(static_cast<int32>(InTexture->Source.GetSizeX()), static_cast<int32>(InTexture->Source.GetSizeY()));
    BaseAtlasBrush.ImageSize = FVector2D(BaseAtlasSize.X, BaseAtlasSize.Y);

    if (const TSharedPtr<FTextureAtlasItem> sameItem = FindItem(InTexture))
    {
        RemoveItems({ sameItem });
    }

    AnalyzeBaseAtlas();

    Options.OutputFolder.Path = FTextureToolkitAssetUtils::GetAssetFolder(InTexture);
    Options.AtlasName = InTexture->GetName();
    Options.bSRGB = static_cast<bool>(InTexture->SRGB);
    Options.CompressionSettings = InTexture->CompressionSettings;
    Options.LODGroup = InTexture->LODGroup;
    Options.MipGenSettings = InTexture->MipGenSettings;
    if (BaseSprites.Num() > 0 && Options.SpriteFolder.Path.IsEmpty())
    {
        Options.SpriteFolder.Path = FTextureToolkitAssetUtils::GetAssetFolder(BaseSprites[0]);
    }
    RefreshOptionsView();

    AutoArrange();
    StatusText = FText::Format(LOCTEXT("BaseSet", "Base atlas {0}: {1} existing sprite(s), {2} occupied region(s). Generate updates it in place."),
        FText::FromString(InTexture->GetName()), FText::AsNumber(BaseSprites.Num()), FText::AsNumber(BaseOccupied.Num()));
}

void STextureAtlasPanel::AddWidgets(const TArray<FAssetData>& InAssets)
{
    TArray<UTexture2D*> textures;
    int32 widgetCount = 0;
    for (const FAssetData& assetData : InAssets)
    {
        UObject* widgetAsset = assetData.GetAsset();
        if (!widgetAsset)
        {
            continue;
        }

        FTextureToolkitAssetUtils::GatherBrushTextures(widgetAsset, textures);
        const bool bListed = AtlasifyWidgets.ContainsByPredicate([&assetData](const TSharedPtr<FAssetData>& InListed)
        {
            return InListed->PackageName == assetData.PackageName;
        });
        if (!bListed)
        {
            AtlasifyWidgets.Add(MakeShared<FAssetData>(assetData));
        }
        ++widgetCount;
    }

    AddTextures(textures);
    RefreshLists();

    FTextureToolkitAssetUtils::Notify(FText::Format(LOCTEXT("WidgetsAdded", "Gathered {0} texture(s) from {1} widget(s)."),
        FText::AsNumber(textures.Num()), FText::AsNumber(widgetCount)), true);
}

FReply STextureAtlasPanel::OnDragOver(const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent)
{
    return FTextureToolkitWidgetUtils::GetDraggedAssets(DragDropEvent).Num() > 0 ? FReply::Handled() : FReply::Unhandled();
}

FReply STextureAtlasPanel::OnDrop(const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent)
{
    const TArray<FAssetData> assets = FTextureToolkitWidgetUtils::GetDraggedAssets(DragDropEvent);
    if (assets.Num() == 0)
    {
        return FReply::Unhandled();
    }

    TArray<FAssetData> widgetAssets;
    for (const FAssetData& assetData : assets)
    {
        if (assetData.IsInstanceOf(UWidgetBlueprint::StaticClass()))
        {
            widgetAssets.Add(assetData);
        }
    }

    AddTextures(FTextureToolkitWidgetUtils::LoadTextures(assets));
    if (widgetAssets.Num() > 0)
    {
        AddWidgets(widgetAssets);
    }
    return FReply::Handled();
}

void STextureAtlasPanel::AddReferencedObjects(FReferenceCollector& Collector)
{
    for (const TSharedPtr<FTextureAtlasItem>& item : Items)
    {
        Collector.AddReferencedObject(item->Texture);
        Collector.AddPropertyReferences(FSlateBrush::StaticStruct(), &item->Brush);
    }
    Collector.AddReferencedObject(BaseAtlas);
    Collector.AddPropertyReferences(FSlateBrush::StaticStruct(), &BaseAtlasBrush);
    Collector.AddReferencedObjects(BaseSprites);
    Collector.AddReferencedObject(GeneratedAtlas);
    Collector.AddReferencedObjects(GeneratedSprites);
}

FString STextureAtlasPanel::GetReferencerName() const
{
    return TEXT("STextureAtlasPanel");
}

#undef LOCTEXT_NAMESPACE
