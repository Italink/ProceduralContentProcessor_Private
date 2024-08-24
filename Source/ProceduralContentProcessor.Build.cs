// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class ProceduralContentProcessor : ModuleRules
{
	public ProceduralContentProcessor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
		

		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				// ... add other public dependencies that you statically link with here ...
			}
			);
		
		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
                "CoreUObject",
                "Engine",
                "BlueprintGraph",
                "UMG",
                "UMGEditor",
                "Slate",
                "SlateCore",
                "PropertyEditor",
                "UnrealEd",
                "AssetRegistry",
                "EditorStyle",
                "InputCore",
                "ContentBrowser",
                "ContentBrowserData",
                "ToolMenus",
                "Projects",
                "LevelEditor",
                "Json",
                "JsonUtilities",
                "Blutility",
                "AssetTools",
                "EditorFramework",
                "WorldPartitionHLODUtilities",
                "WorkspaceMenuStructure",
                "Landscape",
                "Foliage",
                "Kismet",
                "GeometryProcessingInterfaces",
                "DataLayerEditor",
                "MeshModelingToolsExp",
                "ModelingComponentsEditorOnly",
                "InteractiveToolsFramework",
                "EditorInteractiveToolsFramework"
				// ... add private dependencies that you statically link with here ...	
			}
			);
	}
}
