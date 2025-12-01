#pragma once

#include "CoreMinimal.h"
#include "ReprojectionCalibration.h"
#include "Toolkits/AssetEditorToolkit.h"

#include "Calibrator.h"

class SReprojectionCalibrationViewer;
class SReprojectionCalibrationControls;

class FReprojectionCalibrationEditorToolkit : public FAssetEditorToolkit
{
public:
	void InitEditor(const TArray<UObject*>& InObjects);

	virtual void RegisterTabSpawners(const TSharedRef<FTabManager>& InTabManager) override;
	virtual void UnregisterTabSpawners(const TSharedRef<FTabManager>& InTabManager) override;

	virtual FName GetToolkitFName() const override { return "ReprojectionCalibrationEditor"; }
	virtual FText GetBaseToolkitName() const override { return INVTEXT("Reprojection Calibrator"); }
	virtual FString GetWorldCentricTabPrefix() const override { return TEXT("Reprojection Calibrator"); };
	virtual FLinearColor GetWorldCentricTabColorScale() const override { return {}; }

private:
	TSharedRef<SDockTab> HandleTabSpawnerSpawnViewport(const FSpawnTabArgs& Args) const;
	TSharedRef<SDockTab> HandleTabSpawnerSpawnDetails(const FSpawnTabArgs& Args) const;
	TSharedRef<SDockTab> HandleTabSpawnerSpawnControls(const FSpawnTabArgs& Args) const;

	TObjectPtr<UTexture> GetFeedSource() const;
	TObjectPtr<UTexture> GetFeedDestination() const;

	TObjectPtr<UTexture> GetCalibrationImageSource() const;
	TObjectPtr<UTexture> GetCalibrationImageDestination() const;

	void OnCaptureImagePressed();
	void OnResetCalibrationPressed();

	void OnPropertiesFinishedChangingCallback(const FPropertyChangedEvent& Event) const;

	void InvalidateAllViewers() const;

private:
	UReprojectionCalibration* ReprojectionCalibrationAsset = nullptr;

	static const FName ViewerTabId;
	static const FName DetailsTabId;
	static const FName ControlsTabId;

	enum ViewerTypes
	{
		Viewer_Feed = 0,
		Viewer_CalibrationImage,
		Viewer_Count
	};
	TStaticArray<TSharedPtr<SReprojectionCalibrationViewer>, Viewer_Count> ReprojectionCalibrationViewers;
	TSharedPtr<SReprojectionCalibrationControls> ReprojectionCalibrationControls;

	TUniquePtr<FCalibrator> CalibratorImpl;
};
