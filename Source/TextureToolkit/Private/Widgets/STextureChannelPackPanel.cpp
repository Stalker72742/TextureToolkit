// Fill out your copyright notice in the Description page of Project Settings.

#include "Widgets/STextureChannelPackPanel.h"

#include "Engine/Texture2D.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "IStructureDetailsView.h"
#include "PropertyCustomizationHelpers.h"
#include "Styling/AppStyle.h"
#include "ThumbnailRendering/ThumbnailManager.h"
#include "UObject/StructOnScope.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SComboButton.h"
#include "Widgets/Input/SSegmentedControl.h"
#include "Widgets/Input/SSpinBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SSeparator.h"
#include "Widgets/Layout/SSplitter.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"

#include "Core/TextureToolkitAssetUtils.h"
#include "Widgets/TextureToolkitWidgetUtils.h"

#define LOCTEXT_NAMESPACE "STextureChannelPackPanel"

namespace TextureChannelPackPanelPrivate
{
    constexpr int32 PreviewSize = 256;

    FLinearColor GetChannelColor(int32 InChannelIndex)
    {
        switch (InChannelIndex)
        {
        case 0:
            return FLinearColor(0.75f, 0.15f, 0.15f);
        case 1:
            return FLinearColor(0.15f, 0.6f, 0.2f);
        case 2:
            return FLinearColor(0.2f, 0.35f, 0.85f);
        default:
            return FLinearColor(0.45f, 0.45f, 0.45f);
        }
    }

    FText GetChannelLetter(int32 InChannelIndex)
    {
        switch (InChannelIndex)
        {
        case 0:
            return INVTEXT("R");
        case 1:
            return INVTEXT("G");
        case 2:
            return INVTEXT("B");
        default:
            return INVTEXT("A");
        }
    }

    uint8 Quantize(float InValue)
    {
        return static_cast<uint8>(FMath::Clamp(FMath::RoundToInt(InValue * 255.0f), 0, 255));
    }
}

void STextureChannelPackPanel::Construct(const FArguments& InArgs)
{
    using namespace TextureChannelPackPanelPrivate;

    Inputs.SetNum(4);
    FTextureChannelPacker::ApplyPreset(Preset, Inputs);

    PreviewBrush.DrawAs = ESlateBrushDrawType::Image;
    PreviewBrush.ImageSize = FVector2D(PreviewSize, PreviewSize);

    OptionsView = FTextureToolkitWidgetUtils::MakeStructDetails(FTextureChannelPackOptions::StaticStruct(), reinterpret_cast<uint8*>(&Options), OptionsScope);
    OptionsView->GetOnFinishedChangingPropertiesDelegate().AddSP(this, &STextureChannelPackPanel::HandleOptionsChanged);

    TSharedRef<SVerticalBox> channelRows = SNew(SVerticalBox);
    for (int32 channelIndex = 0; channelIndex < 4; ++channelIndex)
    {
        channelRows->AddSlot()
            .AutoHeight()
            .Padding(0.0f, 2.0f)
            [
                BuildChannelRow(channelIndex)
            ];
    }

    ChildSlot
    [
        SNew(SSplitter)
        .Orientation(Orient_Horizontal)

        + SSplitter::Slot()
        .Value(0.6f)
        [
            SNew(SScrollBox)

            + SScrollBox::Slot()
            .Padding(4.0f)
            [
                SNew(SHorizontalBox)

                + SHorizontalBox::Slot()
                .FillWidth(1.0f)
                .VAlign(VAlign_Center)
                [
                    FTextureToolkitWidgetUtils::MakeHeader(LOCTEXT("ChannelsHeader", "Channels"))
                ]

                + SHorizontalBox::Slot()
                .AutoWidth()
                .Padding(4.0f, 0.0f)
                [
                    SNew(SComboButton)
                    .OnGetMenuContent(this, &STextureChannelPackPanel::BuildPresetMenu)
                    .ToolTipText(LOCTEXT("PresetTooltip", "Channel layout preset. Changes labels, default constants and invert flags, textures stay assigned."))
                    .ButtonContent()
                    [
                        SNew(STextBlock)
                        .Text(this, &STextureChannelPackPanel::GetPresetText)
                    ]
                ]

                + SHorizontalBox::Slot()
                .AutoWidth()
                [
                    SNew(SButton)
                    .Text(LOCTEXT("ClearInputs", "Clear"))
                    .OnClicked(this, &STextureChannelPackPanel::HandleClearInputsClicked)
                ]
            ]

            + SScrollBox::Slot()
            .Padding(4.0f, 0.0f)
            [
                channelRows
            ]

            + SScrollBox::Slot()
            .Padding(4.0f, 2.0f)
            [
                SNew(STextBlock)
                .Text(LOCTEXT("ChannelsHint", "Drop several textures here to assign them by name (_AO, _Roughness, _Metallic, _Gloss...). Values are copied as stored, without gamma conversion."))
                .ColorAndOpacity(FSlateColor::UseSubduedForeground())
                .AutoWrapText(true)
            ]

            + SScrollBox::Slot()
            .Padding(4.0f, 10.0f)
            [
                SNew(SSeparator)
            ]

            + SScrollBox::Slot()
            .Padding(4.0f)
            [
                BuildSplitSection()
            ]
        ]

        + SSplitter::Slot()
        .Value(0.4f)
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
                    FTextureToolkitWidgetUtils::MakeHeader(LOCTEXT("PreviewHeader", "Preview"))
                ]

                + SHorizontalBox::Slot()
                .AutoWidth()
                .Padding(4.0f, 0.0f)
                [
                    SNew(SSegmentedControl<int32>)
                    .Value_Lambda([this]() { return PreviewChannel; })
                    .OnValueChanged_Lambda([this](int32 InValue)
                    {
                        PreviewChannel = InValue;
                        bPreviewDirty = true;
                    })

                    + SSegmentedControl<int32>::Slot(INDEX_NONE).Text(INVTEXT("RGB"))
                    + SSegmentedControl<int32>::Slot(0).Text(INVTEXT("R"))
                    + SSegmentedControl<int32>::Slot(1).Text(INVTEXT("G"))
                    + SSegmentedControl<int32>::Slot(2).Text(INVTEXT("B"))
                    + SSegmentedControl<int32>::Slot(3).Text(INVTEXT("A"))
                ]

                + SHorizontalBox::Slot()
                .AutoWidth()
                [
                    SNew(SButton)
                    .ButtonStyle(FAppStyle::Get(), "SimpleButton")
                    .ToolTipText(LOCTEXT("RefreshPreviewTooltip", "Reloads the input textures (after a reimport) and rebuilds the preview."))
                    .OnClicked(this, &STextureChannelPackPanel::HandleRefreshPreviewClicked)
                    [
                        SNew(SImage)
                        .Image(FAppStyle::GetBrush("Icons.Refresh"))
                        .ColorAndOpacity(FSlateColor::UseForeground())
                    ]
                ]
            ]

            + SVerticalBox::Slot()
            .AutoHeight()
            .HAlign(HAlign_Center)
            .Padding(4.0f)
            [
                SNew(SBox)
                .WidthOverride(PreviewSize)
                .HeightOverride(PreviewSize)
                [
                    SNew(SOverlay)

                    + SOverlay::Slot()
                    [
                        SNew(SBorder)
                        .BorderImage(FAppStyle::GetBrush("Brushes.Recessed"))
                    ]

                    + SOverlay::Slot()
                    [
                        SNew(SImage)
                        .Image(&PreviewBrush)
                        .Visibility_Lambda([this]() { return PreviewTexture ? EVisibility::HitTestInvisible : EVisibility::Collapsed; })
                    ]

                    + SOverlay::Slot()
                    .HAlign(HAlign_Center)
                    .VAlign(VAlign_Center)
                    [
                        SNew(STextBlock)
                        .Text(LOCTEXT("NoPreview", "Assign input textures"))
                        .ColorAndOpacity(FSlateColor::UseSubduedForeground())
                        .Visibility_Lambda([this]() { return PreviewTexture ? EVisibility::Collapsed : EVisibility::HitTestInvisible; })
                    ]
                ]
            ]

            + SVerticalBox::Slot()
            .AutoHeight()
            .Padding(4.0f)
            [
                SNew(STextBlock)
                .Text(this, &STextureChannelPackPanel::GetOutputInfoText)
                .AutoWrapText(true)
            ]

            + SVerticalBox::Slot()
            .AutoHeight()
            .Padding(4.0f)
            [
                FTextureToolkitWidgetUtils::MakeHeader(LOCTEXT("OutputHeader", "Output"))
            ]

            + SVerticalBox::Slot()
            .FillHeight(1.0f)
            [
                OptionsView->GetWidget().ToSharedRef()
            ]

            + SVerticalBox::Slot()
            .AutoHeight()
            .Padding(4.0f, 8.0f)
            [
                SNew(SButton)
                .HAlign(HAlign_Center)
                .ButtonStyle(FAppStyle::Get(), "PrimaryButton")
                .Text(LOCTEXT("Create", "Create Packed Texture"))
                .ToolTipText(LOCTEXT("CreateTooltip", "Creates the texture, or overwrites the source of an existing texture with the same name (references stay valid)."))
                .IsEnabled_Lambda([this]()
                {
                    return Inputs.ContainsByPredicate([](const FTextureChannelInput& InInput) { return InInput.Texture != nullptr; });
                })
                .OnClicked(this, &STextureChannelPackPanel::HandleCreateClicked)
            ]
        ]
    ];
}

TSharedRef<SWidget> STextureChannelPackPanel::BuildChannelRow(int32 InChannelIndex)
{
    using namespace TextureChannelPackPanelPrivate;

    auto hasTexture = [this, InChannelIndex]() { return Inputs[InChannelIndex].Texture != nullptr; };

    return SNew(SBorder)
        .BorderImage(FAppStyle::GetBrush("Brushes.Header"))
        .Padding(6.0f)
        [
            SNew(SHorizontalBox)

            + SHorizontalBox::Slot()
            .AutoWidth()
            .VAlign(VAlign_Center)
            .Padding(0.0f, 0.0f, 8.0f, 0.0f)
            [
                SNew(SBox)
                .WidthOverride(26.0f)
                .HeightOverride(26.0f)
                [
                    SNew(SBorder)
                    .BorderImage(FAppStyle::GetBrush("WhiteBrush"))
                    .BorderBackgroundColor(GetChannelColor(InChannelIndex))
                    .HAlign(HAlign_Center)
                    .VAlign(VAlign_Center)
                    [
                        SNew(STextBlock)
                        .Text(GetChannelLetter(InChannelIndex))
                        .Font(FAppStyle::GetFontStyle("NormalFontBold"))
                        .ColorAndOpacity(FLinearColor::White)
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
                .Padding(0.0f, 0.0f, 0.0f, 4.0f)
                [
                    SNew(STextBlock)
                    .Text_Lambda([this, InChannelIndex]() { return Inputs[InChannelIndex].Label; })
                    .Font(FAppStyle::GetFontStyle("NormalFontBold"))
                ]

                + SVerticalBox::Slot()
                .AutoHeight()
                [
                    SNew(SObjectPropertyEntryBox)
                    .AllowedClass(UTexture2D::StaticClass())
                    .ObjectPath_Lambda([this, InChannelIndex]()
                    {
                        const UTexture2D* texture = Inputs[InChannelIndex].Texture;
                        return texture ? texture->GetPathName() : FString();
                    })
                    .OnObjectChanged_Lambda([this, InChannelIndex](const FAssetData& InAssetData)
                    {
                        SetInputTexture(InChannelIndex, Cast<UTexture2D>(InAssetData.GetAsset()));
                    })
                    .ThumbnailPool(UThumbnailManager::Get().GetSharedThumbnailPool())
                    .DisplayThumbnail(true)
                    .AllowClear(true)
                ]
            ]

            + SHorizontalBox::Slot()
            .AutoWidth()
            .VAlign(VAlign_Center)
            .Padding(8.0f, 0.0f, 0.0f, 0.0f)
            [
                SNew(SVerticalBox)

                + SVerticalBox::Slot()
                .AutoHeight()
                .Padding(0.0f, 0.0f, 0.0f, 2.0f)
                [
                    SNew(STextBlock)
                    .Text(LOCTEXT("SourceChannel", "Read channel"))
                    .ColorAndOpacity(FSlateColor::UseSubduedForeground())
                ]

                + SVerticalBox::Slot()
                .AutoHeight()
                [
                    SNew(SSegmentedControl<ETextureToolkitChannel>)
                    .IsEnabled_Lambda(hasTexture)
                    .Value_Lambda([this, InChannelIndex]() { return Inputs[InChannelIndex].SourceChannel; })
                    .OnValueChanged_Lambda([this, InChannelIndex](ETextureToolkitChannel InValue)
                    {
                        Inputs[InChannelIndex].SourceChannel = InValue;
                        bPreviewDirty = true;
                    })

                    + SSegmentedControl<ETextureToolkitChannel>::Slot(ETextureToolkitChannel::Red).Text(INVTEXT("R"))
                    + SSegmentedControl<ETextureToolkitChannel>::Slot(ETextureToolkitChannel::Green).Text(INVTEXT("G"))
                    + SSegmentedControl<ETextureToolkitChannel>::Slot(ETextureToolkitChannel::Blue).Text(INVTEXT("B"))
                    + SSegmentedControl<ETextureToolkitChannel>::Slot(ETextureToolkitChannel::Alpha).Text(INVTEXT("A"))
                    + SSegmentedControl<ETextureToolkitChannel>::Slot(ETextureToolkitChannel::Luminance)
                        .Text(INVTEXT("L"))
                        .ToolTip(LOCTEXT("LuminanceTooltip", "Luminance of RGB"))
                ]

                + SVerticalBox::Slot()
                .AutoHeight()
                .Padding(0.0f, 4.0f, 0.0f, 0.0f)
                [
                    SNew(SHorizontalBox)

                    + SHorizontalBox::Slot()
                    .AutoWidth()
                    .VAlign(VAlign_Center)
                    [
                        SNew(SCheckBox)
                        .IsEnabled_Lambda(hasTexture)
                        .ToolTipText(LOCTEXT("InvertTooltip", "Writes 1 - value. Roughness to Smoothness and back."))
                        .IsChecked_Lambda([this, InChannelIndex]() { return Inputs[InChannelIndex].bInvert ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; })
                        .OnCheckStateChanged_Lambda([this, InChannelIndex](ECheckBoxState InState)
                        {
                            Inputs[InChannelIndex].bInvert = InState == ECheckBoxState::Checked;
                            bPreviewDirty = true;
                        })
                        [
                            SNew(STextBlock)
                            .Text(LOCTEXT("Invert", "Invert"))
                        ]
                    ]

                    + SHorizontalBox::Slot()
                    .AutoWidth()
                    .VAlign(VAlign_Center)
                    .Padding(8.0f, 0.0f, 0.0f, 0.0f)
                    [
                        SNew(SBox)
                        .WidthOverride(80.0f)
                        [
                            SNew(SSpinBox<float>)
                            .MinValue(0.0f)
                            .MaxValue(1.0f)
                            .MinSliderValue(0.0f)
                            .MaxSliderValue(1.0f)
                            .Delta(0.01f)
                            .IsEnabled_Lambda([hasTexture]() { return !hasTexture(); })
                            .ToolTipText(LOCTEXT("ConstantTooltip", "Value written when no texture is assigned."))
                            .Value_Lambda([this, InChannelIndex]() { return Inputs[InChannelIndex].ConstantValue; })
                            .OnValueChanged_Lambda([this, InChannelIndex](float InValue)
                            {
                                Inputs[InChannelIndex].ConstantValue = InValue;
                                bPreviewDirty = true;
                            })
                        ]
                    ]
                ]
            ]
        ];
}

TSharedRef<SWidget> STextureChannelPackPanel::BuildPresetMenu()
{
    FMenuBuilder menuBuilder(true, nullptr);

    const UEnum* presetEnum = StaticEnum<EChannelPackPreset>();
    for (int32 enumIndex = 0; enumIndex < presetEnum->NumEnums() - 1; ++enumIndex)
    {
        const EChannelPackPreset presetValue = static_cast<EChannelPackPreset>(presetEnum->GetValueByIndex(enumIndex));
        menuBuilder.AddMenuEntry(
            presetEnum->GetDisplayNameTextByIndex(enumIndex),
            FText::GetEmpty(),
            FSlateIcon(),
            FUIAction(
                FExecuteAction::CreateSP(this, &STextureChannelPackPanel::SetPreset, presetValue),
                FCanExecuteAction(),
                FIsActionChecked::CreateLambda([this, presetValue]() { return Preset == presetValue; })),
            NAME_None,
            EUserInterfaceActionType::RadioButton);
    }
    return menuBuilder.MakeWidget();
}

TSharedRef<SWidget> STextureChannelPackPanel::BuildSplitSection()
{
    TSharedRef<SHorizontalBox> channelChecks = SNew(SHorizontalBox);
    for (int32 channelIndex = 0; channelIndex < 4; ++channelIndex)
    {
        const uint8 channelBit = static_cast<uint8>(1 << channelIndex);
        channelChecks->AddSlot()
            .AutoWidth()
            .VAlign(VAlign_Center)
            .Padding(0.0f, 0.0f, 10.0f, 0.0f)
            [
                SNew(SCheckBox)
                .IsChecked_Lambda([this, channelBit]() { return (SplitMask & channelBit) ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; })
                .OnCheckStateChanged_Lambda([this, channelBit](ECheckBoxState InState)
                {
                    SplitMask = InState == ECheckBoxState::Checked ? (SplitMask | channelBit) : (SplitMask & ~channelBit);
                })
                [
                    SNew(STextBlock)
                    .Text(TextureChannelPackPanelPrivate::GetChannelLetter(channelIndex))
                ]
            ];
    }

    channelChecks->AddSlot()
        .FillWidth(1.0f)
        .HAlign(HAlign_Right)
        [
            SNew(SButton)
            .Text(LOCTEXT("Split", "Split Into Grayscale Textures"))
            .IsEnabled_Lambda([this]() { return SplitTexture != nullptr && (SplitMask & 0xF) != 0; })
            .OnClicked(this, &STextureChannelPackPanel::HandleSplitClicked)
        ];

    return SNew(SVerticalBox)

        + SVerticalBox::Slot()
        .AutoHeight()
        .Padding(0.0f, 0.0f, 0.0f, 4.0f)
        [
            FTextureToolkitWidgetUtils::MakeHeader(LOCTEXT("SplitHeader", "Split Channels"))
        ]

        + SVerticalBox::Slot()
        .AutoHeight()
        .Padding(0.0f, 0.0f, 0.0f, 4.0f)
        [
            SNew(STextBlock)
            .Text(LOCTEXT("SplitHint", "Writes the selected channels of a texture into separate grayscale textures named <Texture>_R, _G, _B, _A. Uses the Output folder (or the texture folder) and the Save setting."))
            .ColorAndOpacity(FSlateColor::UseSubduedForeground())
            .AutoWrapText(true)
        ]

        + SVerticalBox::Slot()
        .AutoHeight()
        .Padding(0.0f, 0.0f, 0.0f, 4.0f)
        [
            SNew(SObjectPropertyEntryBox)
            .AllowedClass(UTexture2D::StaticClass())
            .ObjectPath_Lambda([this]() { return SplitTexture ? SplitTexture->GetPathName() : FString(); })
            .OnObjectChanged_Lambda([this](const FAssetData& InAssetData) { SetSplitTexture(Cast<UTexture2D>(InAssetData.GetAsset())); })
            .ThumbnailPool(UThumbnailManager::Get().GetSharedThumbnailPool())
            .DisplayThumbnail(true)
            .AllowClear(true)
        ]

        + SVerticalBox::Slot()
        .AutoHeight()
        [
            channelChecks
        ];
}

void STextureChannelPackPanel::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
    SCompoundWidget::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);

    if (bPreviewDirty)
    {
        UpdatePreview();
    }
}

void STextureChannelPackPanel::UpdatePreview()
{
    using namespace TextureChannelPackPanelPrivate;

    bPreviewDirty = false;

    for (auto it = PreviewCache.CreateIterator(); it; ++it)
    {
        UTexture2D* cachedTexture = it.Key().Get();
        const bool bUsed = cachedTexture && Inputs.ContainsByPredicate([cachedTexture](const FTextureChannelInput& InInput)
        {
            return InInput.Texture == cachedTexture;
        });
        if (!bUsed)
        {
            it.RemoveCurrent();
        }
    }

    const bool bHasTexture = Inputs.ContainsByPredicate([](const FTextureChannelInput& InInput) { return InInput.Texture != nullptr; });
    FImage packedImage;
    if (!bHasTexture || !FTextureChannelPacker::Pack(Inputs, FIntPoint(PreviewSize, PreviewSize), &PreviewCache, packedImage))
    {
        PreviewTexture = nullptr;
        PreviewBrush.SetResourceObject(nullptr);
        return;
    }

    const int64 numPixels = packedImage.GetNumPixels();
    TArray64<uint8> previewPixels;
    previewPixels.SetNumUninitialized(numPixels * 4);

    const TArrayView64<const FLinearColor> sourcePixels = static_cast<const FImage&>(packedImage).AsRGBA32F();
    for (int64 pixelIndex = 0; pixelIndex < numPixels; ++pixelIndex)
    {
        const FLinearColor& color = sourcePixels[pixelIndex];
        uint8 red = 0;
        uint8 green = 0;
        uint8 blue = 0;
        if (PreviewChannel == INDEX_NONE)
        {
            red = Quantize(color.R);
            green = Quantize(color.G);
            blue = Quantize(color.B);
        }
        else
        {
            red = Quantize(color.Component(PreviewChannel));
            green = red;
            blue = red;
        }

        uint8* pixel = previewPixels.GetData() + pixelIndex * 4;
        pixel[0] = blue;
        pixel[1] = green;
        pixel[2] = red;
        pixel[3] = 255;
    }

    PreviewTexture = UTexture2D::CreateTransient(PreviewSize, PreviewSize, PF_B8G8R8A8, NAME_None, previewPixels);
    PreviewBrush.SetResourceObject(PreviewTexture);
    PreviewBrush.ImageSize = FVector2D(PreviewSize, PreviewSize);
}

FText STextureChannelPackPanel::GetOutputInfoText() const
{
    FString folder;
    FString assetName;
    if (!FTextureChannelPacker::ResolveOutput(Inputs, Preset, Options, folder, assetName))
    {
        return LOCTEXT("NoOutput", "Assign at least one input texture.");
    }

    const FIntPoint outputSize = FTextureChannelPacker::ComputeOutputSize(Inputs, Options);
    return FText::Format(LOCTEXT("OutputInfo", "Output: {0}/{1}  ({2} x {3})"),
        FText::FromString(folder), FText::FromString(assetName), FText::AsNumber(outputSize.X), FText::AsNumber(outputSize.Y));
}

FText STextureChannelPackPanel::GetPresetText() const
{
    return StaticEnum<EChannelPackPreset>()->GetDisplayNameTextByValue(static_cast<int64>(Preset));
}

void STextureChannelPackPanel::HandleOptionsChanged(const FPropertyChangedEvent& InEvent)
{
    bPreviewDirty = true;
}

FReply STextureChannelPackPanel::HandleCreateClicked()
{
    FText error;
    UTexture2D* packedTexture = FTextureChannelPacker::CreatePackedTexture(Inputs, Preset, Options, error);
    if (packedTexture)
    {
        FTextureToolkitAssetUtils::Notify(FText::Format(LOCTEXT("Created", "Packed texture written: {0}"), FText::FromString(packedTexture->GetPathName())), true);
        FTextureToolkitAssetUtils::BrowseToAssets({ packedTexture });
    }
    else
    {
        FTextureToolkitAssetUtils::Notify(error, false);
    }
    return FReply::Handled();
}

FReply STextureChannelPackPanel::HandleSplitClicked()
{
    TArray<UTexture2D*> createdTextures;
    FText error;
    const bool bSuccess = FTextureChannelPacker::SplitChannels(SplitTexture, SplitMask, Options.OutputFolder.Path, Options.bSaveAssets, createdTextures, error);

    if (createdTextures.Num() > 0)
    {
        FTextureToolkitAssetUtils::Notify(FText::Format(LOCTEXT("SplitDone", "Wrote {0} channel texture(s)."), FText::AsNumber(createdTextures.Num())), true);
        FTextureToolkitAssetUtils::BrowseToAssets(TArray<UObject*>(createdTextures));
    }
    if (!bSuccess)
    {
        FTextureToolkitAssetUtils::Notify(error, false);
    }
    return FReply::Handled();
}

FReply STextureChannelPackPanel::HandleClearInputsClicked()
{
    for (FTextureChannelInput& input : Inputs)
    {
        input.Texture = nullptr;
    }
    bPreviewDirty = true;
    return FReply::Handled();
}

FReply STextureChannelPackPanel::HandleRefreshPreviewClicked()
{
    PreviewCache.Reset();
    bPreviewDirty = true;
    return FReply::Handled();
}

void STextureChannelPackPanel::SetPreset(EChannelPackPreset InPreset)
{
    Preset = InPreset;
    FTextureChannelPacker::ApplyPreset(Preset, Inputs);
    bPreviewDirty = true;
}

void STextureChannelPackPanel::SetInputTexture(int32 InChannelIndex, UTexture2D* InTexture)
{
    if (Inputs.IsValidIndex(InChannelIndex))
    {
        Inputs[InChannelIndex].Texture = InTexture;
        bPreviewDirty = true;
    }
}

void STextureChannelPackPanel::SetRGBSource(UTexture2D* InTexture)
{
    SetPreset(EChannelPackPreset::Custom);
    for (int32 channelIndex = 0; channelIndex < 3; ++channelIndex)
    {
        Inputs[channelIndex].Texture = InTexture;
        Inputs[channelIndex].SourceChannel = static_cast<ETextureToolkitChannel>(channelIndex);
    }
    bPreviewDirty = true;
}

void STextureChannelPackPanel::AutoAssignTextures(const TArray<UTexture2D*>& InTextures)
{
    FTextureChannelPacker::AutoAssign(InTextures, Inputs);
    bPreviewDirty = true;
}

void STextureChannelPackPanel::SetSplitTexture(UTexture2D* InTexture)
{
    SplitTexture = InTexture;
}

FReply STextureChannelPackPanel::OnDragOver(const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent)
{
    return FTextureToolkitWidgetUtils::GetDraggedAssets(DragDropEvent).Num() > 0 ? FReply::Handled() : FReply::Unhandled();
}

FReply STextureChannelPackPanel::OnDrop(const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent)
{
    const TArray<UTexture2D*> textures = FTextureToolkitWidgetUtils::LoadTextures(FTextureToolkitWidgetUtils::GetDraggedAssets(DragDropEvent));
    if (textures.Num() == 0)
    {
        return FReply::Unhandled();
    }

    AutoAssignTextures(textures);
    return FReply::Handled();
}

void STextureChannelPackPanel::AddReferencedObjects(FReferenceCollector& Collector)
{
    for (FTextureChannelInput& input : Inputs)
    {
        Collector.AddReferencedObject(input.Texture);
    }
    Collector.AddReferencedObject(PreviewTexture);
    Collector.AddPropertyReferences(FSlateBrush::StaticStruct(), &PreviewBrush);
    Collector.AddReferencedObject(SplitTexture);
}

FString STextureChannelPackPanel::GetReferencerName() const
{
    return TEXT("STextureChannelPackPanel");
}

#undef LOCTEXT_NAMESPACE
