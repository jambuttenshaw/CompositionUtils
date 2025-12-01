// Copyright Epic Games, Inc. All Rights Reserved.

using System.IO;
using UnrealBuildTool;

public class CompositionUtilsEditor : ModuleRules
{
	public CompositionUtilsEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
		
		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CompositionUtils"
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
                "CoreUObject",
                "Engine",
				"Slate",
				"SlateCore",
                "UnrealEd",

                "RenderCore",
                "Renderer",
                "RHI",
                "RHICore",

                "MediaAssets",
				"OpenCV",
				"OpenCVHelper"
            }
        );

        // OpenCV is used for calibration
        if (Target.Platform == UnrealTargetPlatform.Win64)
        {
            PublicDefinitions.Add("WITH_OPENCV=1");
        }
        else if (Target.Platform == UnrealTargetPlatform.Linux)
        {
            PublicDefinitions.Add("WITH_OPENCV=1");
        }
        else // unsupported platform
        {
            PublicDefinitions.Add("WITH_OPENCV=0");
        }
    }
}
