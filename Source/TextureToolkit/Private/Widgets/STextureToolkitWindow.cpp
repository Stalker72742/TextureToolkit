// Fill out your copyright notice in the Description page of Project Settings.

#include "Widgets/STextureToolkitWindow.h"

#include "AssetRegistry/AssetData.h"
#include "Engine/Texture2D.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Styling/AppStyle.h"
#include "WidgetBlueprint.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SSegmentedControl.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SSplitter.h"
#include "Widgets/Layout/SWidgetSwitcher.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SNullWidget.h"
#include "Widgets/Text/STextBlock.h"

#include "Core/TextureToolkitAssetUtils.h"
#include "Widgets/STextureAtlasPanel.h"
#include "Widgets/STextureChannelPackPanel.h"
#include "Widgets/STextureCompressPanel.h"
#include "Widgets/STextureToolkitBrowser.h"
#include "Widgets/TextureToolkitWidgetUtils.h"

#define LOCTEXT_NAMESPACE "STextureToolkitWindow"

namespace TextureToolkitWindowPrivate
{
    TArray<FAssetData> FilterWidgets(const TArray<FAssetData>& InAssets)
    {
        TArray<FAssetData> widgets;
        for (const FAssetData& assetData : InAssets)
        {
            if (assetData.IsInstanceOf(UWidgetBlueprint::StaticClass()))
            {
                widgets.Add(assetData);
            }
        }
        return widgets;
    }

    TArray<UObject*> LoadAssets(const TArray<FAssetData>& InAssets)
    {
        TArray<UObject*> objects;
        for (const FAssetData& assetData : InAssets)
        {
            if (UObject* asset = assetData.GetAsset())
            {
                objects.Add(asset);
            }
        }
        return objects;
    }
}

void STextureToolkitWindow::Construct(const FArguments& InArgs)
{
    ChildSlot
    [
        SNew(SVerticalBox)

        + SVerticalBox::Slot()
        .AutoHeight()
        [
            SNew(SBorder)
            .BorderImage(FAppStyle::GetBrush("Brushes.Panel"))
            .Padding(FMargin(8.0f, 6.0f))
            [
                SNew(SHorizontalBox)

                + SHorizontalBox::Slot()
                .AutoWidth()
                .VAlign(VAlign_Center)
                [
                    SNew(SSegmentedControl<ETextureToolkitMode>)
                    .Value_Lambda([this]() { return Mode; })
                    .OnValueChanged(this, &STextureToolkitWindow::SetMode)

                    + SSegmentedControl<ETextureToolkitMode>::Slot(ETextureToolkitMode::Compress)
                    .Text(GetModeName(ETextureToolkitMode::Compress))
                    .ToolTip(LOCTEXT("CompressTooltip", "Downscale textures: max in-game size, LOD bias or source resample, plus format overrides."))

                    + SSegmentedControl<ETextureToolkitMode>::Slot(ETextureToolkitMode::ChannelPack)
                    .Text(GetModeName(ETextureToolkitMode::ChannelPack))
                    .ToolTip(LOCTEXT("ChannelPackTooltip", "Pack masks into ORM-like textures, transfer channels between textures, split channels."))

                    + SSegmentedControl<ETextureToolkitMode>::Slot(ETextureToolkitMode::Atlas)
                    .Text(GetModeName(ETextureToolkitMode::Atlas))
                    .ToolTip(LOCTEXT("AtlasTooltip", "Build atlases interactively, generate PaperSprites and switch widget brushes to them."))
                ]

                + SHorizontalBox::Slot()
                .FillWidth(1.0f)
                [
                    SNullWidget::NullWidget
                ]

                + SHorizontalBox::Slot()
                .AutoWidth()
                .VAlign(VAlign_Center)
                [
                    SNew(SButton)
                    .Text_Lambda([this]() { return FText::Format(LOCTEXT("AddSelected", "Add Selected to {0}"), GetModeName(Mode)); })
                    .ToolTipText(LOCTEXT("AddSelectedTooltip", "Adds the assets selected in the browser to the active mode."))
                    .OnClicked(this, &STextureToolkitWindow::HandleAddSelectedClicked)
                ]
            ]
        ]

        + SVerticalBox::Slot()
        .FillHeight(1.0f)
        [
            SNew(SSplitter)
            .Orientation(Orient_Horizontal)

            + SSplitter::Slot()
            .Value(0.32f)
            [
                SAssignNew(Browser, STextureToolkitBrowser)
                .OnGetContextMenu(this, &STextureToolkitWindow::HandleBrowserContextMenu)
                .OnAssetsActivated(this, &STextureToolkitWindow::HandleBrowserActivated)
            ]

            + SSplitter::Slot()
            .Value(0.68f)
            [
                SNew(SWidgetSwitcher)
                .WidgetIndex_Lambda([this]() { return static_cast<int32>(Mode); })

                + SWidgetSwitcher::Slot()
                [
                    SAssignNew(CompressPanel, STextureCompressPanel)
                ]

                + SWidgetSwitcher::Slot()
                [
                    SAssignNew(ChannelPackPanel, STextureChannelPackPanel)
                ]

                + SWidgetSwitcher::Slot()
                [
                    SAssignNew(AtlasPanel, STextureAtlasPanel)
                ]
            ]
        ]
    ];
}

FText STextureToolkitWindow::GetModeName(ETextureToolkitMode InMode)
{
    switch (InMode)
    {
    case ETextureToolkitMode::Compress:
        return LOCTEXT("ModeCompress", "Compress");
    case ETextureToolkitMode::ChannelPack:
        return LOCTEXT("ModeChannelPack", "Channel Pack");
    case ETextureToolkitMode::Atlas:
        return LOCTEXT("ModeAtlas", "Atlas");
    default:
        return FText::GetEmpty();
    }
}

void STextureToolkitWindow::SetMode(ETextureToolkitMode InMode)
{
    Mode = InMode;
}

void STextureToolkitWindow::AddToMode(ETextureToolkitMode InMode, const TArray<FAssetData>& InAssets)
{
    const TArray<UTexture2D*> textures = FTextureToolkitWidgetUtils::LoadTextures(InAssets);

    switch (InMode)
    {
    case ETextureToolkitMode::Compress:
        CompressPanel->AddTextures(textures);
        break;

    case ETextureToolkitMode::ChannelPack:
        ChannelPackPanel->AutoAssignTextures(textures);
        break;

    case ETextureToolkitMode::Atlas:
    {
        AtlasPanel->AddTextures(textures);
        const TArray<FAssetData> widgets = TextureToolkitWindowPrivate::FilterWidgets(InAssets);
        if (widgets.Num() > 0)
        {
            AtlasPanel->AddWidgets(widgets);
        }
        break;
    }

    default:
        break;
    }

    SetMode(InMode);
}

FReply STextureToolkitWindow::HandleAddSelectedClicked()
{
    const TArray<FAssetData> selectedAssets = Browser.IsValid() ? Browser->GetSelectedAssets() : TArray<FAssetData>();
    if (selectedAssets.Num() == 0)
    {
        FTextureToolkitAssetUtils::Notify(LOCTEXT("NothingSelected", "Select assets in the browser first."), false);
        return FReply::Handled();
    }

    AddToMode(Mode, selectedAssets);
    return FReply::Handled();
}

void STextureToolkitWindow::HandleBrowserActivated(const TArray<FAssetData>& InAssets)
{
    const bool bWidgets = TextureToolkitWindowPrivate::FilterWidgets(InAssets).Num() > 0;
    AddToMode(bWidgets ? ETextureToolkitMode::Atlas : Mode, InAssets);
}

TSharedPtr<SWidget> STextureToolkitWindow::HandleBrowserContextMenu(const TArray<FAssetData>& InAssets)
{
    const TArray<FAssetData> assets = InAssets;
    const bool bWidgets = TextureToolkitWindowPrivate::FilterWidgets(assets).Num() > 0;
    const bool bSingle = assets.Num() == 1;

    FMenuBuilder menuBuilder(true, nullptr);

    if (bWidgets)
    {
        menuBuilder.BeginSection(TEXT("Atlas"), LOCTEXT("AtlasSection", "Atlas"));
        menuBuilder.AddMenuEntry(
            LOCTEXT("AtlasifyWidgets", "Atlasify Widgets"),
            LOCTEXT("AtlasifyWidgetsTooltip", "Adds the widgets to the Atlasify list and their brush textures to the atlas. After generation their brushes are switched to the generated PaperSprites."),
            FSlateIcon(FAppStyle::GetAppStyleSetName(), "ClassIcon.WidgetBlueprint"),
            FUIAction(FExecuteAction::CreateLambda([this, assets]() { AddToMode(ETextureToolkitMode::Atlas, assets); })));
        menuBuilder.EndSection();
    }
    else
    {
        menuBuilder.BeginSection(TEXT("Compress"), GetModeName(ETextureToolkitMode::Compress));
        menuBuilder.AddMenuEntry(
            LOCTEXT("AddToCompress", "Add to Compress List"),
            LOCTEXT("AddToCompressTooltip", "Lists the textures in Compress mode with their current and estimated memory."),
            FSlateIcon(),
            FUIAction(FExecuteAction::CreateLambda([this, assets]() { AddToMode(ETextureToolkitMode::Compress, assets); })));
        menuBuilder.EndSection();

        menuBuilder.BeginSection(TEXT("ChannelPack"), GetModeName(ETextureToolkitMode::ChannelPack));
        menuBuilder.AddMenuEntry(
            LOCTEXT("AddToChannelPack", "Add to Channel Pack (assign by name)"),
            LOCTEXT("AddToChannelPackTooltip", "Assigns textures to channels by name (_AO, _Roughness, _Metallic, _Gloss...) and fills empty channels with the rest."),
            FSlateIcon(),
            FUIAction(FExecuteAction::CreateLambda([this, assets]() { AddToMode(ETextureToolkitMode::ChannelPack, assets); })));

        menuBuilder.AddSubMenu(
            LOCTEXT("SetAsInput", "Set as Channel Input"),
            LOCTEXT("SetAsInputTooltip", "Uses the first selected texture as the source of one output channel."),
            FNewMenuDelegate::CreateLambda([this, assets](FMenuBuilder& InSubMenuBuilder)
            {
                static const FText channelNames[4] = { INVTEXT("Red"), INVTEXT("Green"), INVTEXT("Blue"), INVTEXT("Alpha") };
                for (int32 channelIndex = 0; channelIndex < 4; ++channelIndex)
                {
                    InSubMenuBuilder.AddMenuEntry(
                        channelNames[channelIndex],
                        FText::GetEmpty(),
                        FSlateIcon(),
                        FUIAction(FExecuteAction::CreateLambda([this, assets, channelIndex]()
                        {
                            const TArray<UTexture2D*> textures = FTextureToolkitWidgetUtils::LoadTextures(assets);
                            if (textures.Num() > 0)
                            {
                                ChannelPackPanel->SetInputTexture(channelIndex, textures[0]);
                                SetMode(ETextureToolkitMode::ChannelPack);
                            }
                        })));
                }
            }));

        menuBuilder.AddMenuEntry(
            LOCTEXT("UseAsRGB", "Use as RGB Source (channel transfer)"),
            LOCTEXT("UseAsRGBTooltip", "Copies R, G and B from this texture. Assign another texture to Alpha to merge them, e.g. Base Color + Opacity."),
            FSlateIcon(),
            FUIAction(
                FExecuteAction::CreateLambda([this, assets]()
                {
                    const TArray<UTexture2D*> textures = FTextureToolkitWidgetUtils::LoadTextures(assets);
                    if (textures.Num() > 0)
                    {
                        ChannelPackPanel->SetRGBSource(textures[0]);
                        SetMode(ETextureToolkitMode::ChannelPack);
                    }
                }),
                FCanExecuteAction::CreateLambda([bSingle]() { return bSingle; })));

        menuBuilder.AddMenuEntry(
            LOCTEXT("SplitChannels", "Split Channels"),
            LOCTEXT("SplitChannelsTooltip", "Sets this texture as the source of the Split Channels section."),
            FSlateIcon(),
            FUIAction(
                FExecuteAction::CreateLambda([this, assets]()
                {
                    const TArray<UTexture2D*> textures = FTextureToolkitWidgetUtils::LoadTextures(assets);
                    if (textures.Num() > 0)
                    {
                        ChannelPackPanel->SetSplitTexture(textures[0]);
                        SetMode(ETextureToolkitMode::ChannelPack);
                    }
                }),
                FCanExecuteAction::CreateLambda([bSingle]() { return bSingle; })));
        menuBuilder.EndSection();

        menuBuilder.BeginSection(TEXT("Atlas"), GetModeName(ETextureToolkitMode::Atlas));
        menuBuilder.AddMenuEntry(
            LOCTEXT("AddToAtlas", "Add Textures to Atlas"),
            LOCTEXT("AddToAtlasTooltip", "Adds the textures to the atlas sprite list and places them into free space."),
            FSlateIcon(),
            FUIAction(FExecuteAction::CreateLambda([this, assets]() { AddToMode(ETextureToolkitMode::Atlas, assets); })));

        menuBuilder.AddMenuEntry(
            LOCTEXT("UseAsBase", "Use as Base Atlas"),
            LOCTEXT("UseAsBaseTooltip", "Uses this texture as an existing atlas: its pixels and sprites are kept, new sprites go into free space."),
            FSlateIcon(),
            FUIAction(
                FExecuteAction::CreateLambda([this, assets]()
                {
                    const TArray<UTexture2D*> textures = FTextureToolkitWidgetUtils::LoadTextures(assets);
                    if (textures.Num() > 0)
                    {
                        AtlasPanel->SetBaseAtlas(textures[0]);
                        SetMode(ETextureToolkitMode::Atlas);
                    }
                }),
                FCanExecuteAction::CreateLambda([bSingle]() { return bSingle; })));
        menuBuilder.EndSection();
    }

    menuBuilder.BeginSection(TEXT("Asset"), LOCTEXT("AssetSection", "Asset"));
    menuBuilder.AddMenuEntry(
        LOCTEXT("ShowInBrowser", "Show in Content Browser"),
        FText::GetEmpty(),
        FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.Search"),
        FUIAction(FExecuteAction::CreateLambda([assets]() { FTextureToolkitAssetUtils::BrowseToAssets(TextureToolkitWindowPrivate::LoadAssets(assets)); })));
    menuBuilder.AddMenuEntry(
        LOCTEXT("OpenEditor", "Open Editor"),
        FText::GetEmpty(),
        FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.Edit"),
        FUIAction(
            FExecuteAction::CreateLambda([assets]()
            {
                for (UObject* asset : TextureToolkitWindowPrivate::LoadAssets(assets))
                {
                    FTextureToolkitAssetUtils::OpenEditor(asset);
                }
            }),
            FCanExecuteAction::CreateLambda([bSingle]() { return bSingle; })));
    menuBuilder.EndSection();

    return menuBuilder.MakeWidget();
}

#undef LOCTEXT_NAMESPACE
