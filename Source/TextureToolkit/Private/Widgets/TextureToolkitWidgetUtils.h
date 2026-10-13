// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Input/DragAndDrop.h"

class SWidget;
class UTexture2D;
class IStructureDetailsView;
class FStructOnScope;
struct FAssetData;

/**
 * @brief Small Slate helpers shared by the toolkit panels.
 */
class FTextureToolkitWidgetUtils
{
public:
    /**
     * @brief Loads the textures among the given assets.
     * @param InAssets Assets to load.
     * @return Loaded textures.
     */
    static TArray<UTexture2D*> LoadTextures(const TArray<FAssetData>& InAssets);

    /**
     * @brief Returns the assets carried by an asset drag and drop operation.
     * @param InDragDropEvent Drag event.
     * @return Dragged assets, empty for other operations.
     */
    static TArray<FAssetData> GetDraggedAssets(const FDragDropEvent& InDragDropEvent);

    /**
     * @brief Creates a bold section header.
     * @param InText Header text.
     * @return Header widget.
     */
    static TSharedRef<SWidget> MakeHeader(const FText& InText);

    /**
     * @brief Creates a structure details view bound to external struct memory.
     * @param InStruct Struct type.
     * @param InData Struct memory owned by the caller.
     * @param OutScope Created scope, must stay alive with the view.
     * @return The details view.
     */
    static TSharedRef<IStructureDetailsView> MakeStructDetails(const UStruct* InStruct, uint8* InData, TSharedPtr<FStructOnScope>& OutScope);
};
