// Fill out your copyright notice in the Description page of Project Settings.

#include "TextureToolkitModule.h"

#include "Framework/Application/SlateApplication.h"
#include "Framework/Docking/TabManager.h"
#include "Styling/AppStyle.h"
#include "Widgets/Docking/SDockTab.h"
#include "Widgets/STextureToolkitWindow.h"
#include "WorkspaceMenuStructure.h"
#include "WorkspaceMenuStructureModule.h"

#include "TextureToolkitLog.h"

#define LOCTEXT_NAMESPACE "TextureToolkit"

DEFINE_LOG_CATEGORY(LogTextureToolkit);

const FName FTextureToolkitModule::TabName(TEXT("TextureToolkit"));

void FTextureToolkitModule::StartupModule()
{
    FGlobalTabmanager::Get()->RegisterNomadTabSpawner(TabName, FOnSpawnTab::CreateRaw(this, &FTextureToolkitModule::SpawnToolkitTab))
        .SetDisplayName(LOCTEXT("TabTitle", "Texture Toolkit"))
        .SetTooltipText(LOCTEXT("TabTooltip", "Texture optimization tools: downscaling, channel packing and atlas building."))
        .SetGroup(WorkspaceMenu::GetMenuStructure().GetToolsCategory())
        .SetIcon(FSlateIcon(FAppStyle::GetAppStyleSetName(), "ClassIcon.Texture2D"));
}

void FTextureToolkitModule::ShutdownModule()
{
    if (FSlateApplication::IsInitialized())
    {
        FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(TabName);
    }
}

TSharedRef<SDockTab> FTextureToolkitModule::SpawnToolkitTab(const FSpawnTabArgs& InArgs)
{
    return SNew(SDockTab)
        .TabRole(ETabRole::NomadTab)
        [
            SNew(STextureToolkitWindow)
        ];
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FTextureToolkitModule, TextureToolkit)
