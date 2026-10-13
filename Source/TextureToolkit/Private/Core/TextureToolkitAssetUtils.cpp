// Fill out your copyright notice in the Description page of Project Settings.

#include "Core/TextureToolkitAssetUtils.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "ContentBrowserModule.h"
#include "Editor.h"
#include "Engine/Blueprint.h"
#include "Engine/Texture2D.h"
#include "FileHelpers.h"
#include "Framework/Notifications/NotificationManager.h"
#include "IContentBrowserSingleton.h"
#include "ImageCore.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Misc/PackageName.h"
#include "Misc/ScopedSlowTask.h"
#include "PaperSprite.h"
#include "SpriteEditorOnlyTypes.h"
#include "Styling/SlateBrush.h"
#include "Subsystems/AssetEditorSubsystem.h"
#include "UObject/Package.h"
#include "UObject/UObjectHash.h"
#include "UObject/UObjectIterator.h"
#include "UObject/UnrealType.h"
#include "Widgets/Notifications/SNotificationList.h"

#include "TextureToolkitLog.h"

#define LOCTEXT_NAMESPACE "TextureToolkitAssetUtils"

namespace TextureToolkitAssetUtilsPrivate
{
    template<typename TAsset>
    TAsset* FindOrCreateAsset(const FString& InFolder, const FString& InName, bool& bOutCreated, FText& OutError)
    {
        bOutCreated = false;

        const FString folder = FTextureToolkitAssetUtils::NormalizeFolder(InFolder);
        const FString packageName = folder / InName;
        FText reason;
        if (folder.IsEmpty() || InName.IsEmpty() || !FPackageName::IsValidLongPackageName(packageName, false, &reason))
        {
            OutError = FText::Format(LOCTEXT("InvalidPath", "Invalid asset path '{0}'. {1}"), FText::FromString(packageName), reason);
            return nullptr;
        }

        const FString objectPath = packageName + TEXT(".") + InName;
        if (UObject* existing = StaticLoadObject(UObject::StaticClass(), nullptr, *objectPath, nullptr, LOAD_NoWarn | LOAD_Quiet))
        {
            if (TAsset* typedAsset = Cast<TAsset>(existing))
            {
                return typedAsset;
            }
            OutError = FText::Format(LOCTEXT("WrongClass", "'{0}' already exists and is not a {1}."),
                FText::FromString(objectPath), TAsset::StaticClass()->GetDisplayNameText());
            return nullptr;
        }

        if (FPackageName::DoesPackageExist(packageName))
        {
            OutError = FText::Format(LOCTEXT("PackageExists", "Package '{0}' exists but has no asset named '{1}'."),
                FText::FromString(packageName), FText::FromString(InName));
            return nullptr;
        }

        UPackage* package = CreatePackage(*packageName);
        if (!package)
        {
            OutError = FText::Format(LOCTEXT("CreatePackageFailed", "Failed to create package '{0}'."), FText::FromString(packageName));
            return nullptr;
        }

        bOutCreated = true;
        return NewObject<TAsset>(package, *InName, RF_Public | RF_Standalone | RF_Transactional);
    }

    template<typename TFunc>
    int32 ForEachBrush(UObject* InObject, TFunc&& InFunc)
    {
        int32 count = 0;
        for (TPropertyValueIterator<FStructProperty> it(InObject->GetClass(), InObject); it; ++it)
        {
            const FStructProperty* structProperty = it.Key();
            if (!structProperty->Struct || !structProperty->Struct->IsChildOf(FSlateBrush::StaticStruct()))
            {
                continue;
            }

            FSlateBrush* brush = static_cast<FSlateBrush*>(const_cast<void*>(it.Value()));
            if (brush && InFunc(*brush))
            {
                ++count;
            }
        }
        return count;
    }

    UBlueprint* FindOwningBlueprint(UObject* InObject)
    {
        if (UBlueprint* blueprint = Cast<UBlueprint>(InObject))
        {
            return blueprint;
        }
        if (UBlueprint* blueprint = InObject->GetTypedOuter<UBlueprint>())
        {
            return blueprint;
        }
        if (InObject->HasAnyFlags(RF_ClassDefaultObject))
        {
            return UBlueprint::GetBlueprintFromClass(InObject->GetClass());
        }
        if (UClass* outerClass = InObject->GetTypedOuter<UClass>())
        {
            return UBlueprint::GetBlueprintFromClass(outerClass);
        }
        return nullptr;
    }

    IAssetRegistry& GetAssetRegistry()
    {
        return FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
    }
}

FString FTextureToolkitAssetUtils::NormalizeFolder(const FString& InFolder)
{
    FString folder = InFolder.TrimStartAndEnd();
    folder.ReplaceInline(TEXT("\\"), TEXT("/"));
    while (folder.EndsWith(TEXT("/")))
    {
        folder.LeftChopInline(1);
    }

    if (folder.IsEmpty())
    {
        return FString();
    }
    if (!folder.StartsWith(TEXT("/")))
    {
        folder = TEXT("/Game/") + folder;
    }
    return folder;
}

FString FTextureToolkitAssetUtils::GetAssetFolder(const UObject* InAsset)
{
    if (!InAsset)
    {
        return FString();
    }
    return FPackageName::GetLongPackagePath(InAsset->GetPackage()->GetName());
}

FString FTextureToolkitAssetUtils::GetBaseName(const FString& InTextureName)
{
    if (InTextureName.StartsWith(TEXT("T_"), ESearchCase::IgnoreCase) && InTextureName.Len() > 2)
    {
        return InTextureName.RightChop(2);
    }
    return InTextureName;
}

UTexture2D* FTextureToolkitAssetUtils::CreateOrUpdateTexture(const FString& InFolder, const FString& InName, const FImage& InImage, const FTextureAssetSettings& InSettings, FText& OutError)
{
    if (InImage.SizeX <= 0 || InImage.SizeY <= 0)
    {
        OutError = LOCTEXT("EmptyImage", "Cannot create a texture from an empty image.");
        return nullptr;
    }

    bool bCreated = false;
    UTexture2D* texture = TextureToolkitAssetUtilsPrivate::FindOrCreateAsset<UTexture2D>(InFolder, InName, bCreated, OutError);
    if (!texture)
    {
        return nullptr;
    }

    texture->Modify();
    texture->PreEditChange(nullptr);
    texture->Source.Init(InImage);
    texture->CompressionSettings = InSettings.CompressionSettings;
    texture->LODGroup = InSettings.LODGroup;
    texture->MipGenSettings = InSettings.MipGenSettings;
    texture->SRGB = InSettings.bSRGB;
    texture->PostEditChange();

    if (bCreated)
    {
        FAssetRegistryModule::AssetCreated(texture);
    }
    texture->MarkPackageDirty();

    UE_LOG(LogTextureToolkit, Log, TEXT("[FTextureToolkitAssetUtils] %s texture %s (%dx%d)"),
        bCreated ? TEXT("Created") : TEXT("Updated"), *texture->GetPathName(), InImage.SizeX, InImage.SizeY);
    return texture;
}

UPaperSprite* FTextureToolkitAssetUtils::CreateOrUpdateSprite(const FString& InFolder, const FString& InName, UTexture2D* InTexture, const FIntRect& InRect, FText& OutError)
{
    if (!InTexture)
    {
        OutError = LOCTEXT("NoSpriteTexture", "Sprite texture is missing.");
        return nullptr;
    }

    bool bCreated = false;
    UPaperSprite* sprite = TextureToolkitAssetUtilsPrivate::FindOrCreateAsset<UPaperSprite>(InFolder, InName, bCreated, OutError);
    if (!sprite)
    {
        return nullptr;
    }

    sprite->Modify();
    if (bCreated)
    {
        if (FByteProperty* collisionProperty = FindFProperty<FByteProperty>(UPaperSprite::StaticClass(), TEXT("SpriteCollisionDomain")))
        {
            collisionProperty->SetPropertyValue_InContainer(sprite, static_cast<uint8>(ESpriteCollisionMode::None));
        }
    }

    FSpriteAssetInitParameters initParams;
    initParams.Texture = InTexture;
    initParams.Offset = InRect.Min;
    initParams.Dimension = InRect.Size();
    sprite->InitializeSprite(initParams);
    sprite->PostEditChange();

    if (bCreated)
    {
        FAssetRegistryModule::AssetCreated(sprite);
    }
    sprite->MarkPackageDirty();
    return sprite;
}

void FTextureToolkitAssetUtils::SaveAssets(const TArray<UObject*>& InAssets)
{
    TArray<UPackage*> packages;
    for (UObject* asset : InAssets)
    {
        if (IsValid(asset))
        {
            packages.AddUnique(asset->GetPackage());
        }
    }

    if (packages.Num() > 0)
    {
        UEditorLoadingAndSavingUtils::SavePackages(packages, true);
    }
}

void FTextureToolkitAssetUtils::FindSpritesUsingTexture(UTexture2D* InTexture, TArray<UPaperSprite*>& OutSprites)
{
    if (!InTexture)
    {
        return;
    }

    IAssetRegistry& assetRegistry = TextureToolkitAssetUtilsPrivate::GetAssetRegistry();
    const FTopLevelAssetPath spriteClassPath = UPaperSprite::StaticClass()->GetClassPathName();

    TArray<FName> referencers;
    assetRegistry.GetReferencers(InTexture->GetPackage()->GetFName(), referencers);
    for (const FName& packageName : referencers)
    {
        TArray<FAssetData> assets;
        assetRegistry.GetAssetsByPackageName(packageName, assets);
        for (const FAssetData& assetData : assets)
        {
            if (assetData.AssetClassPath != spriteClassPath)
            {
                continue;
            }
            UPaperSprite* sprite = Cast<UPaperSprite>(assetData.GetAsset());
            if (sprite && sprite->GetSourceTexture() == InTexture)
            {
                OutSprites.AddUnique(sprite);
            }
        }
    }

    for (TObjectIterator<UPaperSprite> it; it; ++it)
    {
        UPaperSprite* sprite = *it;
        if (IsValid(sprite) && !sprite->HasAnyFlags(RF_Transient | RF_ClassDefaultObject) && sprite->GetSourceTexture() == InTexture)
        {
            OutSprites.AddUnique(sprite);
        }
    }
}

void FTextureToolkitAssetUtils::GatherBrushTextures(UObject* InAsset, TArray<UTexture2D*>& OutTextures)
{
    if (!InAsset)
    {
        return;
    }

    TArray<UObject*> objects;
    GetObjectsWithPackage(InAsset->GetPackage(), objects);
    for (UObject* object : objects)
    {
        if (!IsValid(object))
        {
            continue;
        }

        TextureToolkitAssetUtilsPrivate::ForEachBrush(object, [&OutTextures](FSlateBrush& InBrush)
        {
            if (UTexture2D* texture = Cast<UTexture2D>(InBrush.GetResourceObject()))
            {
                OutTextures.AddUnique(texture);
                return true;
            }
            return false;
        });
    }
}

void FTextureToolkitAssetUtils::ReplaceBrushResources(const TMap<UObject*, UObject*>& InReplacements, const TArray<FName>& InPackages, const TSet<FName>& InExcludedPackages, bool bInSave, FTextureReplaceReport& OutReport)
{
    if (InReplacements.Num() == 0)
    {
        return;
    }

    IAssetRegistry& assetRegistry = TextureToolkitAssetUtilsPrivate::GetAssetRegistry();

    TSet<FName> packageNames;
    if (InPackages.Num() > 0)
    {
        packageNames.Append(InPackages);
    }
    else
    {
        for (const TPair<UObject*, UObject*>& replacement : InReplacements)
        {
            if (!replacement.Key)
            {
                continue;
            }
            TArray<FName> referencers;
            assetRegistry.GetReferencers(replacement.Key->GetPackage()->GetFName(), referencers,
                UE::AssetRegistry::EDependencyCategory::Package, UE::AssetRegistry::EDependencyQuery::Hard);
            packageNames.Append(referencers);
        }
    }

    for (const FName& excludedPackage : InExcludedPackages)
    {
        packageNames.Remove(excludedPackage);
    }

    FScopedSlowTask slowTask(static_cast<float>(packageNames.Num()), LOCTEXT("ReplacingReferences", "Replacing texture references..."));
    slowTask.MakeDialog(true);

    TSet<UBlueprint*> blueprintsToCompile;
    TArray<UPackage*> modifiedPackages;

    for (const FName& packageName : packageNames)
    {
        slowTask.EnterProgressFrame(1.0f, FText::FromName(packageName));
        if (slowTask.ShouldCancel())
        {
            break;
        }

        const FString packageString = packageName.ToString();
        if (packageString.StartsWith(TEXT("/Script/")) || packageString.StartsWith(TEXT("/Engine/")) || packageString.StartsWith(TEXT("/Temp/")))
        {
            continue;
        }

        UPackage* package = LoadPackage(nullptr, *packageString, LOAD_NoWarn | LOAD_Quiet);
        if (!package)
        {
            continue;
        }
        ++OutReport.PackagesScanned;

        TArray<UObject*> objects;
        GetObjectsWithPackage(package, objects);

        bool bPackageModified = false;
        for (UObject* object : objects)
        {
            if (!IsValid(object))
            {
                continue;
            }

            bool bObjectModified = false;
            const int32 replacedCount = TextureToolkitAssetUtilsPrivate::ForEachBrush(object, [&InReplacements, &bObjectModified, object](FSlateBrush& InBrush)
            {
                UObject* const* replacement = InReplacements.Find(InBrush.GetResourceObject());
                if (!replacement || !*replacement)
                {
                    return false;
                }
                if (!bObjectModified)
                {
                    object->Modify();
                    bObjectModified = true;
                }
                InBrush.SetResourceObject(*replacement);
                return true;
            });

            if (replacedCount > 0)
            {
                OutReport.BrushesReplaced += replacedCount;
                bPackageModified = true;
                if (UBlueprint* blueprint = TextureToolkitAssetUtilsPrivate::FindOwningBlueprint(object))
                {
                    blueprintsToCompile.Add(blueprint);
                }
            }
        }

        if (bPackageModified)
        {
            package->MarkPackageDirty();
            modifiedPackages.Add(package);
            UE_LOG(LogTextureToolkit, Log, TEXT("[FTextureToolkitAssetUtils] Replaced brush textures in %s"), *packageString);
        }
    }

    OutReport.PackagesModified = modifiedPackages.Num();

    for (UBlueprint* blueprint : blueprintsToCompile)
    {
        if (!IsValid(blueprint))
        {
            continue;
        }
        FBlueprintEditorUtils::MarkBlueprintAsModified(blueprint);
        FKismetEditorUtilities::CompileBlueprint(blueprint);
        ++OutReport.BlueprintsCompiled;
    }

    if (bInSave && modifiedPackages.Num() > 0)
    {
        UEditorLoadingAndSavingUtils::SavePackages(modifiedPackages, true);
    }
}

void FTextureToolkitAssetUtils::BrowseToAssets(const TArray<UObject*>& InAssets)
{
    if (InAssets.Num() == 0)
    {
        return;
    }
    FContentBrowserModule& contentBrowserModule = FModuleManager::LoadModuleChecked<FContentBrowserModule>(TEXT("ContentBrowser"));
    contentBrowserModule.Get().SyncBrowserToAssets(InAssets);
}

void FTextureToolkitAssetUtils::OpenEditor(UObject* InAsset)
{
    if (InAsset && GEditor)
    {
        GEditor->GetEditorSubsystem<UAssetEditorSubsystem>()->OpenEditorForAsset(InAsset);
    }
}

void FTextureToolkitAssetUtils::Notify(const FText& InText, bool bInSuccess)
{
    FNotificationInfo info(InText);
    info.ExpireDuration = 6.0f;
    info.bUseSuccessFailIcons = true;
    info.bFireAndForget = true;

    TSharedPtr<SNotificationItem> notification = FSlateNotificationManager::Get().AddNotification(info);
    if (notification.IsValid())
    {
        notification->SetCompletionState(bInSuccess ? SNotificationItem::CS_Success : SNotificationItem::CS_Fail);
    }

    if (bInSuccess)
    {
        UE_LOG(LogTextureToolkit, Log, TEXT("[TextureToolkit] %s"), *InText.ToString());
    }
    else
    {
        UE_LOG(LogTextureToolkit, Warning, TEXT("[TextureToolkit] %s"), *InText.ToString());
    }
}

#undef LOCTEXT_NAMESPACE
