#include "ReprojectionCalibrationActions.h"
#include "ReprojectionCalibration.h"
#include "ReprojectionCalibrationEditorToolkit.h"

UClass* FReprojectionCalibrationActions::GetSupportedClass() const
{
	return UReprojectionCalibration::StaticClass();
}

FText FReprojectionCalibrationActions::GetName() const
{
	return INVTEXT("Reprojection Calibration");
}

FColor FReprojectionCalibrationActions::GetTypeColor() const
{
	return FColor::Emerald;
}

uint32 FReprojectionCalibrationActions::GetCategories()
{
	return EAssetTypeCategories::Misc;
}

void FReprojectionCalibrationActions::OpenAssetEditor(const TArray<UObject*>& InObjects, TSharedPtr<IToolkitHost> EditWithinLevelEditor)
{
	MakeShared<FReprojectionCalibrationEditorToolkit>()->InitEditor(InObjects);
}
