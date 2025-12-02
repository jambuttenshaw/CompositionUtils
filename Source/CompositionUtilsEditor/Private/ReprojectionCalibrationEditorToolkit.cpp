#include "ReprojectionCalibrationEditorToolkit.h"

#include "CompositionUtilsEditor.h"
#include "Widgets/SReprojectionCalibrationControls.h"
#include "Widgets/SReprojectionCalibrationViewer.h"
#include "Widgets/Layout/SScaleBox.h"

#define LOCTEXT_NAMESPACE "FCompositionUtilsEditorModule"


const FName FReprojectionCalibrationEditorToolkit::ViewerTabId = "ReprojectionCalibrationViewerTab";
const FName FReprojectionCalibrationEditorToolkit::DetailsTabId = "ReprojectionCalibrationDetailsTab";
const FName FReprojectionCalibrationEditorToolkit::ControlsTabId = "ReprojectionCalibrationControlsTab";


void FReprojectionCalibrationEditorToolkit::InitEditor(const TArray<UObject*>& InObjects)
{
	Asset = Cast<UReprojectionCalibration>(InObjects[0]);

	ReprojectionCalibrationViewers[Viewer_Feed] = SNew(SReprojectionCalibrationViewer)
		.SourceTexture(this, &FReprojectionCalibrationEditorToolkit::GetFeedSource)
		.DestinationTexture(this, &FReprojectionCalibrationEditorToolkit::GetFeedDestination);
	ReprojectionCalibrationViewers[Viewer_CalibrationImage] = SNew(SReprojectionCalibrationViewer)
		.SourceTexture(this, &FReprojectionCalibrationEditorToolkit::GetCalibrationImageSource)
		.DestinationTexture(this, &FReprojectionCalibrationEditorToolkit::GetCalibrationImageDestination);

	ReprojectionCalibrationControls = SNew(SReprojectionCalibrationControls)
		.OnCaptureImagePressed(this, &FReprojectionCalibrationEditorToolkit::OnCaptureImagePressed)
		.OnResetCalibrationPressed(this, &FReprojectionCalibrationEditorToolkit::OnResetCalibrationPressed);

	CalibratorImpl = MakeUnique<FCalibrator>();

	const TSharedRef<FTabManager::FLayout> Layout = FTabManager::NewLayout("ReprojectionCalibrationEditorLayout_v3")
	->AddArea(
		FTabManager::NewPrimaryArea()
		->SetOrientation(Orient_Horizontal)
		->Split(
			FTabManager::NewSplitter()
			->SetOrientation(Orient_Vertical)
			->SetSizeCoefficient(0.8f)
			->Split
			(
				FTabManager::NewStack()
				->SetSizeCoefficient(0.6f)
				->AddTab(ViewerTabId, ETabState::OpenedTab)
				->SetHideTabWell(true)
			)
			->Split(
				FTabManager::NewStack()
				->SetSizeCoefficient(0.4f)
				->AddTab(ControlsTabId, ETabState::OpenedTab)
			)
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

	InTabManager->RegisterTabSpawner(ViewerTabId, FOnSpawnTab::CreateSP(this, &FReprojectionCalibrationEditorToolkit::HandleTabSpawnerSpawnViewport))
		.SetDisplayName(INVTEXT("Viewer"))
		.SetGroup((WorkspaceMenuCategory.ToSharedRef()));

	InTabManager->RegisterTabSpawner(DetailsTabId, FOnSpawnTab::CreateSP(this, &FReprojectionCalibrationEditorToolkit::HandleTabSpawnerSpawnDetails))
		.SetDisplayName(INVTEXT("Details"))
		.SetGroup(WorkspaceMenuCategory.ToSharedRef());

	InTabManager->RegisterTabSpawner(ControlsTabId, FOnSpawnTab::CreateSP(this, &FReprojectionCalibrationEditorToolkit::HandleTabSpawnerSpawnControls))
		.SetDisplayName(INVTEXT("Controls"))
		.SetGroup(WorkspaceMenuCategory.ToSharedRef());
}

void FReprojectionCalibrationEditorToolkit::UnregisterTabSpawners(const TSharedRef<FTabManager>& InTabManager)
{
	FAssetEditorToolkit::UnregisterTabSpawners(InTabManager);
	InTabManager->UnregisterTabSpawner(ViewerTabId);
	InTabManager->UnregisterTabSpawner(DetailsTabId);
	InTabManager->UnregisterTabSpawner(ControlsTabId);
}

TSharedRef<SDockTab> FReprojectionCalibrationEditorToolkit::HandleTabSpawnerSpawnViewport(const FSpawnTabArgs& Args) const
{
	check(Args.GetTabId() == ViewerTabId);

	return SNew(SDockTab)
	[
		SNew(SScaleBox)
			.Stretch(EStretch::ScaleToFit)
			.StretchDirection(EStretchDirection::Both)
			.HAlign(HAlign_Center)
			.VAlign(VAlign_Top)
			[
				SNew(SVerticalBox)
					+ SVerticalBox::Slot()
					.FillHeight(1.0f)
					.HAlign(HAlign_Fill)
					.VAlign(VAlign_Fill)
					[
						ReprojectionCalibrationViewers[Viewer_Feed].ToSharedRef()
					]
				+ SVerticalBox::Slot()
					.FillHeight(1.0f)
					.HAlign(HAlign_Fill)
					.VAlign(VAlign_Fill)
					[
						ReprojectionCalibrationViewers[Viewer_CalibrationImage].ToSharedRef()
					]
			]
	];
			
}

TSharedRef<SDockTab> FReprojectionCalibrationEditorToolkit::HandleTabSpawnerSpawnDetails(const FSpawnTabArgs& Args) const
{
	check(Args.GetTabId() == DetailsTabId);

	FPropertyEditorModule& PropertyEditorModule = FModuleManager::GetModuleChecked<FPropertyEditorModule>("PropertyEditor");

	FDetailsViewArgs DetailsViewArgs;
	DetailsViewArgs.NameAreaSettings = FDetailsViewArgs::HideNameArea;

	TSharedRef<IDetailsView> DetailsView = PropertyEditorModule.CreateDetailView(DetailsViewArgs);
	DetailsView->SetObjects(TArray<UObject*>{ Asset });
	DetailsView->OnFinishedChangingProperties().AddSP(this, &FReprojectionCalibrationEditorToolkit::OnPropertiesFinishedChangingCallback);

	return SNew(SDockTab)
		[
			DetailsView
		];
}

TSharedRef<SDockTab> FReprojectionCalibrationEditorToolkit::HandleTabSpawnerSpawnControls(const FSpawnTabArgs& Args) const
{
	check(Args.GetTabId() == ControlsTabId);

	return SNew(SDockTab)
	[
		ReprojectionCalibrationControls.ToSharedRef()
	];
}

TObjectPtr<UTexture> FReprojectionCalibrationEditorToolkit::GetFeedSource() const
{
	check(Asset);
	return Asset->Source ? Asset->Source->GetTexture() : nullptr;
}

TObjectPtr<UTexture> FReprojectionCalibrationEditorToolkit::GetFeedDestination() const
{
	check(Asset);
	return Asset->Destination ? Asset->Destination->GetTexture() : nullptr;
}

TObjectPtr<UTexture> FReprojectionCalibrationEditorToolkit::GetCalibrationImageSource() const
{
	TObjectPtr<UTexture> CalibratedSource = CalibratorImpl->GetCalibratedSourceDebugView();
	return CalibratedSource ? CalibratedSource : GetFeedSource();
}

TObjectPtr<UTexture> FReprojectionCalibrationEditorToolkit::GetCalibrationImageDestination() const
{
	TObjectPtr<UTexture> CalibratedDestination = CalibratorImpl->GetCalibratedDestinationDebugView();
	return CalibratedDestination ? CalibratedDestination : GetFeedDestination();
}

void FReprojectionCalibrationEditorToolkit::OnCaptureImagePressed()
{
	if (!Asset)
		return;

	FTransform OutTransform;
	FCalibrator::ECalibrationResult Result = CalibratorImpl->RunCalibration(
		Asset->Source, 
		Asset->Destination,
		Asset->CheckerboardDimensions,
		Asset->CheckerboardSize,
		OutTransform);

	if (Result == FCalibrator::ECalibrationResult::Success)
	{
		ReprojectionCalibrationViewers[Viewer_CalibrationImage]->InvalidateBrushes();

		Asset->ExtrinsicTransform = OutTransform;
		(void)Asset->MarkPackageDirty();
	}
	else
	{
		FText ErrorMessage = FCalibrator::GetErrorTextForResult(Result);
		UE_LOG(LogCompositionUtilsEditor, Error, TEXT("Calibration Error: %s"), *ErrorMessage.ToString())
	}
}

void FReprojectionCalibrationEditorToolkit::OnResetCalibrationPressed()
{
	if (!Asset)
		return;

	CalibratorImpl->RestartCalibration();
	ReprojectionCalibrationViewers[Viewer_CalibrationImage]->InvalidateBrushes();
}

void FReprojectionCalibrationEditorToolkit::OnPropertiesFinishedChangingCallback(const FPropertyChangedEvent& Event) const
{
	FName PropertyName = Event.GetPropertyName();

	bool bTargetChanged = false;
	bTargetChanged |= PropertyName == GET_MEMBER_NAME_CHECKED(UReprojectionCalibration, Source)
				   || PropertyName == GET_MEMBER_NAME_CHECKED(UReprojectionCalibration, Destination);

	// Check if inner members of Source or Destination were changed
	if (Asset && !bTargetChanged)
	{
		// Don't know type of abstract objects Source and Destination so cannot directly check members
		// Invalidate brushes if any member variables of source or destination were changed is conservative
		int32 NumObjectsEdited = Event.GetNumObjectsBeingEdited();
		for (int32 Index = 0; Index < NumObjectsEdited; Index++)
		{
			const UObject* EditedObject = Event.GetObjectBeingEdited(Index);
			bTargetChanged |= EditedObject == static_cast<const UObject*>(Asset->Source)
						   || EditedObject == static_cast<const UObject*>(Asset->Destination);
		}
	}

	if (bTargetChanged)
	{
		InvalidateAllViewers();

		// If target itself has changed, it may be different resolution, must also release transient resources
		CalibratorImpl->RestartCalibration();
		CalibratorImpl->ResetTransientResources();
	}
}

void FReprojectionCalibrationEditorToolkit::InvalidateAllViewers() const
{
	for (const auto& Viewer : ReprojectionCalibrationViewers)
	{
		Viewer->InvalidateBrushes();
	}
}

#undef LOCTEXT_NAMESPACE