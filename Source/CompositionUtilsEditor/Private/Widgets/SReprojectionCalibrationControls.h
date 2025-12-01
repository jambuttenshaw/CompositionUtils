#pragma once

#include "CoreMinimal.h"

DECLARE_DELEGATE(FCaptureImage)
DECLARE_DELEGATE(FResetCalibration)

class SReprojectionCalibrationControls : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SReprojectionCalibrationControls)
		{}
		SLATE_EVENT(FCaptureImage, OnCaptureImagePressed)
		SLATE_EVENT(FResetCalibration, OnResetCalibrationPressed)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

private:
	FCaptureImage OnCaptureImagePressed;
	FResetCalibration OnResetCalibrationPressed;
};
