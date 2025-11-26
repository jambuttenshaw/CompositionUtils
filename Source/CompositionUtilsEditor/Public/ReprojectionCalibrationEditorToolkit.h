#pragma once

#include "CoreMinimal.h"
#include "ReprojectionCalibration.h"
#include "Toolkits/AssetEditorToolkit.h"


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
	UReprojectionCalibration* ReprojectionCalibrationAsset = nullptr;

};
