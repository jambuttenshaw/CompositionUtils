#pragma once

#include "CoreMinimal.h"
#include "ReprojectionCalibration.h"
#include "Toolkits/AssetEditorToolkit.h"


class SReprojectionCalibrationViewWidget;

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

	TObjectPtr<UTexture> GetSource() const;
	TObjectPtr<UTexture> GetDestination() const;

	void OnPropertiesFinishedChangingCallback(const FPropertyChangedEvent& Event) const;

private:
	UReprojectionCalibration* ReprojectionCalibrationAsset = nullptr;

	static const FName ViewportTabId;
	static const FName DetailsTabId;

	TSharedPtr<SReprojectionCalibrationViewWidget> ReprojectionCalibrationViewport;
};
