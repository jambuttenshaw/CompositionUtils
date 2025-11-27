#include "ReprojectionCalibrationEditorToolkit.h"
#include "SReprojectionCalibrationViewWidget.h"

#define LOCTEXT_NAMESPACE "FCompositionUtilsEditorModule"


const FName FReprojectionCalibrationEditorToolkit::ViewportTabId = "ReprojectionCalibrationViewportTab";
const FName FReprojectionCalibrationEditorToolkit::DetailsTabId = "ReprojectionCalibrationDetailsTab";


void FReprojectionCalibrationEditorToolkit::InitEditor(const TArray<UObject*>& InObjects)
{
	ReprojectionCalibrationAsset = Cast<UReprojectionCalibration>(InObjects[0]);

	ReprojectionCalibrationViewport = SNew(SReprojectionCalibrationViewWidget)
										.SourceTexture(this, &FReprojectionCalibrationEditorToolkit::GetSource)
										.DestinationTexture(this, &FReprojectionCalibrationEditorToolkit::GetDestination);

	const TSharedRef<FTabManager::FLayout> Layout = FTabManager::NewLayout("ReprojectionCalibrationEditorLayout")
	->AddArea(
		FTabManager::NewPrimaryArea()->SetOrientation(Orient_Horizontal)
		->Split(
			FTabManager::NewStack()
			->SetSizeCoefficient(0.8f)
			->AddTab(ViewportTabId, ETabState::OpenedTab)
			->SetHideTabWell(true)
		)
		->Split(
			FTabManager::NewStack()
			->SetSizeCoefficient(0.2f)
			->AddTab(DetailsTabId, ETabState::OpenedTab)
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

	InTabManager->RegisterTabSpawner(ViewportTabId, FOnSpawnTab::CreateSP(this, &FReprojectionCalibrationEditorToolkit::HandleTabSpawnerSpawnViewport))
		.SetDisplayName(INVTEXT("Dynamic Brush Test"))
		.SetGroup((WorkspaceMenuCategory.ToSharedRef()));

	InTabManager->RegisterTabSpawner(DetailsTabId, FOnSpawnTab::CreateSP(this, &FReprojectionCalibrationEditorToolkit::HandleTabSpawnerSpawnDetails))
		.SetDisplayName(INVTEXT("Details"))
		.SetGroup(WorkspaceMenuCategory.ToSharedRef());
}

void FReprojectionCalibrationEditorToolkit::UnregisterTabSpawners(const TSharedRef<FTabManager>& InTabManager)
{
	FAssetEditorToolkit::UnregisterTabSpawners(InTabManager);
	InTabManager->UnregisterTabSpawner(ViewportTabId);
	InTabManager->UnregisterTabSpawner(DetailsTabId);
}

TSharedRef<SDockTab> FReprojectionCalibrationEditorToolkit::HandleTabSpawnerSpawnViewport(const FSpawnTabArgs& Args) const
{
	check(Args.GetTabId() == ViewportTabId);

	return SNew(SDockTab)
		[
			ReprojectionCalibrationViewport.ToSharedRef()
		];
}

TSharedRef<SDockTab> FReprojectionCalibrationEditorToolkit::HandleTabSpawnerSpawnDetails(const FSpawnTabArgs& Args) const
{
	check(Args.GetTabId() == DetailsTabId);

	FPropertyEditorModule& PropertyEditorModule = FModuleManager::GetModuleChecked<FPropertyEditorModule>("PropertyEditor");

	FDetailsViewArgs DetailsViewArgs;
	DetailsViewArgs.NameAreaSettings = FDetailsViewArgs::HideNameArea;

	TSharedRef<IDetailsView> DetailsView = PropertyEditorModule.CreateDetailView(DetailsViewArgs);
	DetailsView->SetObjects(TArray<UObject*>{ ReprojectionCalibrationAsset });
	DetailsView->OnFinishedChangingProperties().AddSP(this, &FReprojectionCalibrationEditorToolkit::OnPropertiesFinishedChangingCallback);

	return SNew(SDockTab)
		[
			DetailsView
		];
}

TObjectPtr<UObject> FReprojectionCalibrationEditorToolkit::GetSource() const
{
	check(ReprojectionCalibrationAsset);
	return ReprojectionCalibrationAsset->Source;
}

TObjectPtr<UObject> FReprojectionCalibrationEditorToolkit::GetDestination() const
{
	check(ReprojectionCalibrationAsset);
	return ReprojectionCalibrationAsset->Destination;
}

void FReprojectionCalibrationEditorToolkit::OnPropertiesFinishedChangingCallback(const FPropertyChangedEvent& Event) const
{
	FName PropertyName = Event.GetPropertyName();
	if (PropertyName == GET_MEMBER_NAME_CHECKED(UReprojectionCalibration, Source)
	 || PropertyName == GET_MEMBER_NAME_CHECKED(UReprojectionCalibration, Destination))
	{
		ReprojectionCalibrationViewport->InvalidateBrushes();
	}
}


#undef LOCTEXT_NAMESPACE