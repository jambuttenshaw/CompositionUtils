// Copyright Epic Games, Inc. All Rights Reserved.

#include "CompositionUtils.h"

#define LOCTEXT_NAMESPACE "FCompositionUtilsModule"

DEFINE_LOG_CATEGORY(LogCompositionUtils);


void FCompositionUtilsModule::StartupModule()
{
	FString PluginDir = FPaths::Combine(FPaths::ProjectPluginsDir(), TEXT("CompositionUtils"));
	FString ShaderDirectory = FPaths::Combine(PluginDir, TEXT("Shaders"));
	AddShaderSourceDirectoryMapping("/Plugin/CompositionUtils", ShaderDirectory);
}

void FCompositionUtilsModule::ShutdownModule()
{
}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FCompositionUtilsModule, CompositionUtils)