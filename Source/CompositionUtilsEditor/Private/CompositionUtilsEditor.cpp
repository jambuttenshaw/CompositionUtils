// Copyright Epic Games, Inc. All Rights Reserved.

#include "CompositionUtilsEditor.h"

#define LOCTEXT_NAMESPACE "FCompositionUtilsEditorModule"


void FCompositionUtilsEditorModule::StartupModule()
{
	ReprojectionCalibrationActions = MakeShared<FReprojectionCalibrationActions>();
	FAssetToolsModule::GetModule().Get().RegisterAssetTypeActions(ReprojectionCalibrationActions.ToSharedRef());
}

void FCompositionUtilsEditorModule::ShutdownModule()
{
	if (!FModuleManager::Get().IsModuleLoaded("AssetTools")) return;
	FAssetToolsModule::GetModule().Get().UnregisterAssetTypeActions(ReprojectionCalibrationActions.ToSharedRef());
}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FCompositionUtilsEditorModule, CompositionUtilsEditor)