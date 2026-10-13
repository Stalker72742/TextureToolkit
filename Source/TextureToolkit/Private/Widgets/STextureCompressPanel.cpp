// Fill out your copyright notice in the Description page of Project Settings.

#include "Widgets/STextureCompressPanel.h"

#include "Engine/Texture2D.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "IStructureDetailsView.h"
#include "Misc/MessageDialog.h"
#include "Misc/ScopedSlowTask.h"
#include "ScopedTransaction.h"
#include "Styling/AppStyle.h"
#include "UObject/StructOnScope.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SSplitter.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Views/SHeaderRow.h"
#include "Widgets/Views/STableRow.h"

#include "Core/TextureToolkitAssetUtils.h"
#include "Core/TextureToolkitImageUtils.h"
#include "Widgets/TextureToolkitWidgetUtils.h"

#define LOCTEXT_NAMESPACE "STextureCompressPanel"

namespace TextureCompressPanelPrivate
{
    const FName ColumnTexture(TEXT("Texture"));
    const FName ColumnSource(TEXT("Source"));
    const FName ColumnCurrent(TEXT("Current"));
    const FName ColumnFormat(TEXT("Format"));
    const FName ColumnMemory(TEXT("Memory"));
    const FName ColumnNewSize(TEXT("NewSize"));
    const FName ColumnNewMemory(TEXT("NewMemory"));
    const FName ColumnNote(TEXT("Note"));

    FText SizeText(const FIntPoint& InSize)
    {
        return FText::FromString(FString::Printf(TEXT("%d x %d"), InSize.X, InSize.Y));
    }

    FText FormatText(EPixelFormat InFormat)
    {
        if (InFormat == PF_Unknown || InFormat >= PF_MAX)
        {
            return FText::GetEmpty();
        }
        FString formatName = GPixelFormats[InFormat].Name;
        formatName.RemoveFromStart(TEXT("PF_"));
        return FText::FromString(formatName);
    }

    class STextureCompressRow : public SMultiColumnTableRow<TSharedPtr<FTextureCompressEntry>>
    {
    public:
        SLATE_BEGIN_ARGS(STextureCompressRow) {}
            SLATE_ARGUMENT(TSharedPtr<FTextureCompressEntry>, Entry)
        SLATE_END_ARGS()

        void Construct(const FArguments& InArgs, const TSharedRef<STableViewBase>& InOwnerTable)
        {
            Entry = InArgs._Entry;
            SMultiColumnTableRow<TSharedPtr<FTextureCompressEntry>>::Construct(FSuperRowType::FArguments(), InOwnerTable);
        }

        virtual TSharedRef<SWidget> GenerateWidgetForColumn(const FName& InColumnName) override
        {
            TWeakPtr<FTextureCompressEntry> weakEntry = Entry;
            TAttribute<FText> text;

            if (InColumnName == ColumnTexture)
            {
                text = TAttribute<FText>::CreateLambda([weakEntry]()
                {
                    const TSharedPtr<FTextureCompressEntry> entry = weakEntry.Pin();
                    return entry.IsValid() ? FText::FromString(GetNameSafe(entry->Texture)) : FText::GetEmpty();
                });
            }
            else if (InColumnName == ColumnSource)
            {
                text = TAttribute<FText>::CreateLambda([weakEntry]()
                {
                    const TSharedPtr<FTextureCompressEntry> entry = weakEntry.Pin();
                    return entry.IsValid() ? SizeText(entry->Plan.SourceSize) : FText::GetEmpty();
                });
            }
            else if (InColumnName == ColumnCurrent)
            {
                text = TAttribute<FText>::CreateLambda([weakEntry]()
                {
                    const TSharedPtr<FTextureCompressEntry> entry = weakEntry.Pin();
                    return entry.IsValid() ? SizeText(entry->Plan.CurrentSize) : FText::GetEmpty();
                });
            }
            else if (InColumnName == ColumnFormat)
            {
                text = TAttribute<FText>::CreateLambda([weakEntry]()
                {
                    const TSharedPtr<FTextureCompressEntry> entry = weakEntry.Pin();
                    if (!entry.IsValid())
                    {
                        return FText::GetEmpty();
                    }
                    if (entry->Plan.NewFormat != entry->Plan.CurrentFormat)
                    {
                        return FText::Format(LOCTEXT("FormatChange", "{0} > {1}"), FormatText(entry->Plan.CurrentFormat), FormatText(entry->Plan.NewFormat));
                    }
                    return FormatText(entry->Plan.CurrentFormat);
                });
            }
            else if (InColumnName == ColumnMemory)
            {
                text = TAttribute<FText>::CreateLambda([weakEntry]()
                {
                    const TSharedPtr<FTextureCompressEntry> entry = weakEntry.Pin();
                    return entry.IsValid() ? FText::FromString(FTextureToolkitImageUtils::FormatBytes(entry->Plan.CurrentBytes)) : FText::GetEmpty();
                });
            }
            else if (InColumnName == ColumnNewSize)
            {
                text = TAttribute<FText>::CreateLambda([weakEntry]()
                {
                    const TSharedPtr<FTextureCompressEntry> entry = weakEntry.Pin();
                    return entry.IsValid() ? SizeText(entry->Plan.NewSize) : FText::GetEmpty();
                });
            }
            else if (InColumnName == ColumnNewMemory)
            {
                return SNew(STextBlock)
                    .Text_Lambda([weakEntry]()
                    {
                        const TSharedPtr<FTextureCompressEntry> entry = weakEntry.Pin();
                        return entry.IsValid() ? FText::FromString(FTextureToolkitImageUtils::FormatBytes(entry->Plan.NewBytes)) : FText::GetEmpty();
                    })
                    .ColorAndOpacity_Lambda([weakEntry]()
                    {
                        const TSharedPtr<FTextureCompressEntry> entry = weakEntry.Pin();
                        if (entry.IsValid() && entry->Plan.NewBytes < entry->Plan.CurrentBytes)
                        {
                            return FSlateColor(FLinearColor(0.35f, 0.85f, 0.35f));
                        }
                        if (entry.IsValid() && entry->Plan.NewBytes > entry->Plan.CurrentBytes)
                        {
                            return FSlateColor(FLinearColor(0.95f, 0.45f, 0.3f));
                        }
                        return FSlateColor::UseForeground();
                    });
            }
            else if (InColumnName == ColumnNote)
            {
                return SNew(STextBlock)
                    .Text_Lambda([weakEntry]()
                    {
                        const TSharedPtr<FTextureCompressEntry> entry = weakEntry.Pin();
                        return entry.IsValid() ? entry->Plan.Note : FText::GetEmpty();
                    })
                    .ColorAndOpacity_Lambda([weakEntry]()
                    {
                        const TSharedPtr<FTextureCompressEntry> entry = weakEntry.Pin();
                        if (entry.IsValid() && entry->Plan.bInvalid)
                        {
                            return FSlateColor(FLinearColor(0.95f, 0.3f, 0.3f));
                        }
                        return entry.IsValid() && entry->Plan.bChanged ? FSlateColor::UseForeground() : FSlateColor::UseSubduedForeground();
                    });
            }

            return SNew(STextBlock).Text(text);
        }

    protected:
        TSharedPtr<FTextureCompressEntry> Entry;
    };
}

void STextureCompressPanel::Construct(const FArguments& InArgs)
{
    using namespace TextureCompressPanelPrivate;

    OptionsView = FTextureToolkitWidgetUtils::MakeStructDetails(FTextureCompressOptions::StaticStruct(), reinterpret_cast<uint8*>(&Options), OptionsScope);
    OptionsView->GetOnFinishedChangingPropertiesDelegate().AddSP(this, &STextureCompressPanel::HandleOptionsChanged);

    ChildSlot
    [
        SNew(SSplitter)
        .Orientation(Orient_Horizontal)

        + SSplitter::Slot()
        .Value(0.68f)
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
                    FTextureToolkitWidgetUtils::MakeHeader(LOCTEXT("TexturesHeader", "Textures"))
                ]

                + SHorizontalBox::Slot()
                .AutoWidth()
                .Padding(2.0f, 0.0f)
                [
                    SNew(SButton)
                    .Text(LOCTEXT("Refresh", "Refresh"))
                    .ToolTipText(LOCTEXT("RefreshTooltip", "Recomputes sizes and formats, for example after textures finished compiling."))
                    .OnClicked(this, &STextureCompressPanel::HandleRefreshClicked)
                ]

                + SHorizontalBox::Slot()
                .AutoWidth()
                .Padding(2.0f, 0.0f)
                [
                    SNew(SButton)
                    .Text(LOCTEXT("RemoveUnchanged", "Remove Unchanged"))
                    .OnClicked(this, &STextureCompressPanel::HandleRemoveUnchangedClicked)
                ]

                + SHorizontalBox::Slot()
                .AutoWidth()
                .Padding(2.0f, 0.0f)
                [
                    SNew(SButton)
                    .Text(LOCTEXT("RemoveSelected", "Remove Selected"))
                    .OnClicked(this, &STextureCompressPanel::HandleRemoveSelectedClicked)
                ]

                + SHorizontalBox::Slot()
                .AutoWidth()
                .Padding(2.0f, 0.0f)
                [
                    SNew(SButton)
                    .Text(LOCTEXT("Clear", "Clear"))
                    .OnClicked(this, &STextureCompressPanel::HandleClearClicked)
                ]
            ]

            + SVerticalBox::Slot()
            .FillHeight(1.0f)
            .Padding(4.0f, 0.0f)
            [
                SNew(SBorder)
                .BorderImage(FAppStyle::GetBrush("Brushes.Recessed"))
                .Padding(0.0f)
                [
                    SAssignNew(ListView, SListView<TSharedPtr<FTextureCompressEntry>>)
                    .ListItemsSource(&Entries)
                    .SelectionMode(ESelectionMode::Multi)
                    .OnGenerateRow(this, &STextureCompressPanel::HandleGenerateRow)
                    .OnContextMenuOpening(this, &STextureCompressPanel::HandleListContextMenu)
                    .OnMouseButtonDoubleClick(this, &STextureCompressPanel::HandleListDoubleClick)
                    .HeaderRow
                    (
                        SNew(SHeaderRow)
                        + SHeaderRow::Column(ColumnTexture).DefaultLabel(LOCTEXT("ColTexture", "Texture")).FillWidth(0.22f)
                        + SHeaderRow::Column(ColumnSource).DefaultLabel(LOCTEXT("ColSource", "Source")).FillWidth(0.1f)
                        + SHeaderRow::Column(ColumnCurrent).DefaultLabel(LOCTEXT("ColCurrent", "In Game")).FillWidth(0.1f)
                        + SHeaderRow::Column(ColumnFormat).DefaultLabel(LOCTEXT("ColFormat", "Format")).FillWidth(0.11f)
                        + SHeaderRow::Column(ColumnMemory).DefaultLabel(LOCTEXT("ColMemory", "Memory")).FillWidth(0.09f)
                        + SHeaderRow::Column(ColumnNewSize).DefaultLabel(LOCTEXT("ColNewSize", "New Size")).FillWidth(0.1f)
                        + SHeaderRow::Column(ColumnNewMemory).DefaultLabel(LOCTEXT("ColNewMemory", "New Memory")).FillWidth(0.09f)
                        + SHeaderRow::Column(ColumnNote).DefaultLabel(LOCTEXT("ColNote", "Changes")).FillWidth(0.19f)
                    )
                ]
            ]

            + SVerticalBox::Slot()
            .AutoHeight()
            .Padding(4.0f)
            [
                SNew(STextBlock)
                .Text(this, &STextureCompressPanel::GetSummaryText)
                .AutoWrapText(true)
            ]
        ]

        + SSplitter::Slot()
        .Value(0.32f)
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

            + SVerticalBox::Slot()
            .AutoHeight()
            .Padding(4.0f, 8.0f)
            [
                SNew(SButton)
                .HAlign(HAlign_Center)
                .ButtonStyle(FAppStyle::Get(), "PrimaryButton")
                .Text(LOCTEXT("Apply", "Apply To Listed Textures"))
                .ToolTipText(LOCTEXT("ApplyTooltip", "Applies the settings to every listed texture with changes. Undoable until saved, Resize Source rewrites the source pixels."))
                .IsEnabled_Lambda([this]() { return Entries.Num() > 0; })
                .OnClicked(this, &STextureCompressPanel::HandleApplyClicked)
            ]
        ]
    ];
}

void STextureCompressPanel::AddTextures(const TArray<UTexture2D*>& InTextures)
{
    for (UTexture2D* texture : InTextures)
    {
        if (!texture)
        {
            continue;
        }

        const bool bExists = Entries.ContainsByPredicate([texture](const TSharedPtr<FTextureCompressEntry>& InEntry)
        {
            return InEntry->Texture == texture;
        });
        if (bExists)
        {
            continue;
        }

        TSharedPtr<FTextureCompressEntry> entry = MakeShared<FTextureCompressEntry>();
        entry->Texture = texture;
        entry->Plan = FTextureCompressor::BuildPlan(texture, Options);
        Entries.Add(entry);
    }

    if (ListView.IsValid())
    {
        ListView->RequestListRefresh();
    }
}

void STextureCompressPanel::RefreshPlans()
{
    Entries.RemoveAll([](const TSharedPtr<FTextureCompressEntry>& InEntry)
    {
        return !InEntry.IsValid() || !IsValid(InEntry->Texture);
    });

    for (const TSharedPtr<FTextureCompressEntry>& entry : Entries)
    {
        entry->Plan = FTextureCompressor::BuildPlan(entry->Texture, Options);
    }

    if (ListView.IsValid())
    {
        ListView->RequestListRefresh();
    }
}

TSharedRef<ITableRow> STextureCompressPanel::HandleGenerateRow(TSharedPtr<FTextureCompressEntry> InEntry, const TSharedRef<STableViewBase>& InOwnerTable)
{
    return SNew(TextureCompressPanelPrivate::STextureCompressRow, InOwnerTable)
        .Entry(InEntry);
}

void STextureCompressPanel::HandleOptionsChanged(const FPropertyChangedEvent& InEvent)
{
    RefreshPlans();
}

TSharedPtr<SWidget> STextureCompressPanel::HandleListContextMenu()
{
    const TArray<UObject*> selectedTextures = GetSelectedTextures();
    if (selectedTextures.Num() == 0)
    {
        return nullptr;
    }

    FMenuBuilder menuBuilder(true, nullptr);
    menuBuilder.AddMenuEntry(
        LOCTEXT("Browse", "Show in Content Browser"),
        FText::GetEmpty(),
        FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.Search"),
        FUIAction(FExecuteAction::CreateLambda([selectedTextures]() { FTextureToolkitAssetUtils::BrowseToAssets(selectedTextures); })));

    menuBuilder.AddMenuEntry(
        LOCTEXT("RemoveFromList", "Remove From List"),
        FText::GetEmpty(),
        FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.Delete"),
        FUIAction(FExecuteAction::CreateLambda([this]() { HandleRemoveSelectedClicked(); })));

    return menuBuilder.MakeWidget();
}

void STextureCompressPanel::HandleListDoubleClick(TSharedPtr<FTextureCompressEntry> InEntry)
{
    if (InEntry.IsValid())
    {
        FTextureToolkitAssetUtils::OpenEditor(InEntry->Texture);
    }
}

FReply STextureCompressPanel::HandleApplyClicked()
{
    RefreshPlans();

    TArray<TSharedPtr<FTextureCompressEntry>> changedEntries;
    for (const TSharedPtr<FTextureCompressEntry>& entry : Entries)
    {
        if (entry->Plan.bChanged && !entry->Plan.bInvalid)
        {
            changedEntries.Add(entry);
        }
    }

    if (changedEntries.Num() == 0)
    {
        FTextureToolkitAssetUtils::Notify(LOCTEXT("NothingToApply", "Nothing to change with the current settings."), false);
        return FReply::Handled();
    }

    if (Options.Method == ETextureCompressMethod::ResizeSource)
    {
        const FText message = FText::Format(
            LOCTEXT("ConfirmResize", "Resample the source pixels of {0} texture(s)?\n\nThis can be undone until the textures are saved. After saving the original resolution is lost."),
            FText::AsNumber(changedEntries.Num()));
        if (FMessageDialog::Open(EAppMsgType::YesNo, message, LOCTEXT("ConfirmResizeTitle", "Resize Source")) != EAppReturnType::Yes)
        {
            return FReply::Handled();
        }
    }

    int64 bytesBefore = 0;
    int64 bytesAfter = 0;
    TArray<UObject*> modifiedTextures;
    {
        FScopedTransaction transaction(LOCTEXT("CompressTransaction", "Texture Toolkit: Compress Textures"));
        FScopedSlowTask slowTask(static_cast<float>(changedEntries.Num()), LOCTEXT("Applying", "Applying texture settings..."));
        slowTask.MakeDialog(true);

        for (const TSharedPtr<FTextureCompressEntry>& entry : changedEntries)
        {
            slowTask.EnterProgressFrame(1.0f, FText::FromString(GetNameSafe(entry->Texture)));
            if (slowTask.ShouldCancel())
            {
                break;
            }

            FText error;
            if (FTextureCompressor::ApplyPlan(entry->Texture, entry->Plan, Options, error))
            {
                bytesBefore += entry->Plan.CurrentBytes;
                bytesAfter += entry->Plan.NewBytes;
                modifiedTextures.Add(entry->Texture);
            }
            else if (!error.IsEmpty())
            {
                FTextureToolkitAssetUtils::Notify(error, false);
            }
        }
    }

    if (Options.bSaveAfterApply)
    {
        FTextureToolkitAssetUtils::SaveAssets(modifiedTextures);
    }

    RefreshPlans();

    FTextureToolkitAssetUtils::Notify(FText::Format(
        LOCTEXT("Applied", "Updated {0} texture(s). Estimated memory {1} > {2}."),
        FText::AsNumber(modifiedTextures.Num()),
        FText::FromString(FTextureToolkitImageUtils::FormatBytes(bytesBefore)),
        FText::FromString(FTextureToolkitImageUtils::FormatBytes(bytesAfter))), modifiedTextures.Num() > 0);

    return FReply::Handled();
}

FReply STextureCompressPanel::HandleRemoveSelectedClicked()
{
    if (!ListView.IsValid())
    {
        return FReply::Handled();
    }

    const TArray<TSharedPtr<FTextureCompressEntry>> selectedEntries = ListView->GetSelectedItems();
    Entries.RemoveAll([&selectedEntries](const TSharedPtr<FTextureCompressEntry>& InEntry)
    {
        return selectedEntries.Contains(InEntry);
    });
    ListView->ClearSelection();
    ListView->RequestListRefresh();
    return FReply::Handled();
}

FReply STextureCompressPanel::HandleRemoveUnchangedClicked()
{
    Entries.RemoveAll([](const TSharedPtr<FTextureCompressEntry>& InEntry)
    {
        return !InEntry->Plan.bChanged;
    });
    if (ListView.IsValid())
    {
        ListView->RequestListRefresh();
    }
    return FReply::Handled();
}

FReply STextureCompressPanel::HandleClearClicked()
{
    Entries.Reset();
    if (ListView.IsValid())
    {
        ListView->RequestListRefresh();
    }
    return FReply::Handled();
}

FReply STextureCompressPanel::HandleRefreshClicked()
{
    RefreshPlans();
    return FReply::Handled();
}

FText STextureCompressPanel::GetSummaryText() const
{
    int64 currentBytes = 0;
    int64 newBytes = 0;
    int32 changedCount = 0;
    for (const TSharedPtr<FTextureCompressEntry>& entry : Entries)
    {
        currentBytes += entry->Plan.CurrentBytes;
        newBytes += entry->Plan.NewBytes;
        changedCount += entry->Plan.bChanged ? 1 : 0;
    }

    if (Entries.Num() == 0)
    {
        return LOCTEXT("EmptyHint", "Add textures from the browser: select them and use the context menu, double click, or drag them here.");
    }

    return FText::Format(
        LOCTEXT("Summary", "{0} texture(s), {1} with changes. Estimated memory: {2} > {3} (saves {4})."),
        FText::AsNumber(Entries.Num()),
        FText::AsNumber(changedCount),
        FText::FromString(FTextureToolkitImageUtils::FormatBytes(currentBytes)),
        FText::FromString(FTextureToolkitImageUtils::FormatBytes(newBytes)),
        FText::FromString(FTextureToolkitImageUtils::FormatBytes(FMath::Max<int64>(0, currentBytes - newBytes))));
}

TArray<UObject*> STextureCompressPanel::GetSelectedTextures() const
{
    TArray<UObject*> textures;
    if (ListView.IsValid())
    {
        for (const TSharedPtr<FTextureCompressEntry>& entry : ListView->GetSelectedItems())
        {
            if (entry.IsValid() && entry->Texture)
            {
                textures.Add(entry->Texture);
            }
        }
    }
    return textures;
}

FReply STextureCompressPanel::OnDragOver(const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent)
{
    return FTextureToolkitWidgetUtils::GetDraggedAssets(DragDropEvent).Num() > 0 ? FReply::Handled() : FReply::Unhandled();
}

FReply STextureCompressPanel::OnDrop(const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent)
{
    const TArray<FAssetData> assets = FTextureToolkitWidgetUtils::GetDraggedAssets(DragDropEvent);
    if (assets.Num() == 0)
    {
        return FReply::Unhandled();
    }

    AddTextures(FTextureToolkitWidgetUtils::LoadTextures(assets));
    return FReply::Handled();
}

void STextureCompressPanel::AddReferencedObjects(FReferenceCollector& Collector)
{
    for (const TSharedPtr<FTextureCompressEntry>& entry : Entries)
    {
        if (entry.IsValid())
        {
            Collector.AddReferencedObject(entry->Texture);
        }
    }
}

FString STextureCompressPanel::GetReferencerName() const
{
    return TEXT("STextureCompressPanel");
}

#undef LOCTEXT_NAMESPACE
