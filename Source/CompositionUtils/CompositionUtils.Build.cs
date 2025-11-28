// Copyright Epic Games, Inc. All Rights Reserved.

using System.IO;
using UnrealBuildTool;

public class CompositionUtils : ModuleRules
{
	public CompositionUtils(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
		
		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
                "Composure"
			}
			);

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
                "CoreUObject",
                "Engine",
                "RenderCore",
                "Renderer",
                "RHI",
                "RHICore",
                "MediaAssets"
            }
			);

        string EnginePath = Path.GetFullPath(Target.RelativeEnginePath);
        PrivateIncludePaths.AddRange(
            new string[]
            {
                EnginePath + "Source/Runtime/Renderer/Private/",
                EnginePath + "Source/Runtime/Renderer/Internal/",
            }
        );
    }
}
