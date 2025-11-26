#include "ReprojectionCalibrationEditorToolkit.h"


void FReprojectionCalibrationEditorToolkit::InitEditor(const TArray<UObject*>& InObjects)
{
	ReprojectionCalibrationAsset = Cast<UReprojectionCalibration>(InObjects[0]);

	const TSharedRef<FTabManager::FLayout> Layout = FTabManager::NewLayout("ReprojectionCalibrationEditorLayout")
	->AddArea(
		FTabManager::NewPrimaryArea()->SetOrientation(Orient_Vertical)
		->Split(
			FTabManager::NewStack()
			->SetSizeCoefficient(1.0f)
			->AddTab("ReprojectionCalibrationDetailsTab", ETabState::OpenedTab)
		)
	);

	FAssetEditorToolkit::InitAssetEditor(
		EToolkitMode::Standalone,
		{},
		"ReprojectionCalibrationEditor",
		Layout, 
		true,
		true,
		InObjects);
}

void FReprojectionCalibrationEditorToolkit::RegisterTabSpawners(const TSharedRef<FTabManager>& InTabManager)
{
	FAssetEditorToolkit::RegisterTabSpawners(InTabManager);

	WorkspaceMenuCategory = InTabManager->AddLocalWorkspaceMenuCategory(INVTEXT("Reprojection Calibrator"));

	FPropertyEditorModule& PropertyEditorModule = FModuleManager::GetModuleChecked<FPropertyEditorModule>("PropertyEditor");
	FDetailsViewArgs DetailsViewArgs;
	DetailsViewArgs.NameAreaSettings = FDetailsViewArgs::HideNameArea;
	TSharedRef<IDetailsView> DetailsView = PropertyEditorModule.CreateDetailView(DetailsViewArgs);
	DetailsView->SetObjects(TArray<UObject*>{ ReprojectionCalibrationAsset });
	InTabManager->RegisterTabSpawner("ReprojectionCalibrationDetailsTab", FOnSpawnTab::CreateLambda([=](const FSpawnTabArgs&)
		{
			return SNew(SDockTab)
				[
					DetailsView
				];
		}))
		.SetDisplayName(INVTEXT("Details"))
		.SetGroup(WorkspaceMenuCategory.ToSharedRef());
}

void FReprojectionCalibrationEditorToolkit::UnregisterTabSpawners(const TSharedRef<FTabManager>& InTabManager)
{
	FAssetEditorToolkit::UnregisterTabSpawners(InTabManager);
	InTabManager->UnregisterTabSpawner("ReprojectionCalibrationDetailsTab");
}
