// Fill out your copyright notice in the Description page of Project Settings.

using UnrealBuildTool;

public class TextureToolkit : ModuleRules
{
	public TextureToolkit(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
				"Engine",
			}
			);

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"Slate",
				"SlateCore",
				"InputCore",
				"UnrealEd",
				"ContentBrowser",
				"ContentBrowserData",
				"ToolWidgets",
				"AssetRegistry",
				"AssetTools",
				"PropertyEditor",
				"WorkspaceMenuStructure",
				"ImageCore",
				"Paper2D",
				"UMG",
				"UMGEditor",
			}
			);
	}
}
