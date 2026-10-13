// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

class SDockTab;
class FSpawnTabArgs;

/**
 * @brief Editor module of the Texture Toolkit.
 * @note Registers a nomad tab in the Tools menu that hosts the texture browser and the
 *       Compress / Channel Pack / Atlas modes.
 */
class FTextureToolkitModule : public IModuleInterface
{
public:
    /** @brief Name of the nomad tab hosting the toolkit window. */
    static const FName TabName;

protected:
    /**
     * @brief Spawns the toolkit tab.
     * @param InArgs Tab spawn arguments.
     * @return The new dock tab.
     */
    TSharedRef<SDockTab> SpawnToolkitTab(const FSpawnTabArgs& InArgs);

public:
    // --- IModuleInterface ---

    virtual void StartupModule() override;
    virtual void ShutdownModule() override;
};
