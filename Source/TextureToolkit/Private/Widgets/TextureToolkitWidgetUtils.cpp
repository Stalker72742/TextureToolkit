// Fill out your copyright notice in the Description page of Project Settings.

#include "Widgets/TextureToolkitWidgetUtils.h"

#include "AssetRegistry/AssetData.h"
#include "DragAndDrop/AssetDragDropOp.h"
#include "Engine/Texture2D.h"
#include "IDetailsView.h"
#include "IStructureDetailsView.h"
#include "Modules/ModuleManager.h"
#include "PropertyEditorModule.h"
#include "Styling/AppStyle.h"
#include "UObject/StructOnScope.h"
#include "Widgets/Text/STextBlock.h"

TArray<UTexture2D*> FTextureToolkitWidgetUtils::LoadTextures(const TArray<FAssetData>& InAssets)
{
    TArray<UTexture2D*> textures;
    for (const FAssetData& assetData : InAssets)
    {
        if (!assetData.IsInstanceOf(UTexture2D::StaticClass()))
        {
            continue;
        }
        if (UTexture2D* texture = Cast<UTexture2D>(assetData.GetAsset()))
        {
            textures.AddUnique(texture);
        }
    }
    return textures;
}

TArray<FAssetData> FTextureToolkitWidgetUtils::GetDraggedAssets(const FDragDropEvent& InDragDropEvent)
{
    if (TSharedPtr<FAssetDragDropOp> assetOperation = InDragDropEvent.GetOperationAs<FAssetDragDropOp>())
    {
        return assetOperation->GetAssets();
    }
    return TArray<FAssetData>();
}

TSharedRef<SWidget> FTextureToolkitWidgetUtils::MakeHeader(const FText& InText)
{
    return SNew(STextBlock)
        .Text(InText)
        .Font(FAppStyle::GetFontStyle("NormalFontBold"));
}

TSharedRef<IStructureDetailsView> FTextureToolkitWidgetUtils::MakeStructDetails(const UStruct* InStruct, uint8* InData, TSharedPtr<FStructOnScope>& OutScope)
{
    FPropertyEditorModule& propertyEditorModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>(TEXT("PropertyEditor"));

    FDetailsViewArgs detailsViewArgs;
    detailsViewArgs.bAllowSearch = false;
    detailsViewArgs.bHideSelectionTip = true;
    detailsViewArgs.bShowOptions = false;
    detailsViewArgs.NameAreaSettings = FDetailsViewArgs::HideNameArea;

    FStructureDetailsViewArgs structureViewArgs;

    OutScope = MakeShared<FStructOnScope>(InStruct, InData);
    return propertyEditorModule.CreateStructureDetailView(detailsViewArgs, structureViewArgs, OutScope);
}
