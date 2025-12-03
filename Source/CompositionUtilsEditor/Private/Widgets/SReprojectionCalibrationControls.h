#pragma once

#include "CoreMinimal.h"


DECLARE_DELEGATE(FReprojectionCalibrationButtonClicked)

/**
 * Control panel master widget, containing action buttons and info text blocks.
 */
class SReprojectionCalibrationControls : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SReprojectionCalibrationControls)
		{}
		SLATE_ATTRIBUTE(FText, GetNumRunsText)
		SLATE_ATTRIBUTE(FText, GetAvgSourceErrorText)
		SLATE_ATTRIBUTE(FText, GetAvgDestErrorText)

		SLATE_EVENT(FReprojectionCalibrationButtonClicked, OnCaptureClicked)
		SLATE_EVENT(FReprojectionCalibrationButtonClicked, OnRestartClicked)
		SLATE_EVENT(FReprojectionCalibrationButtonClicked, OnResetClicked)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	void AddSuccessToLog(int32 SampleIndex, double SourceError, double DestError, const FTransform& CalibratedTransform, double Weight) const;
	void AddErrorToLog(const FText& ErrorText) const;
	void AddToLog(const TSharedPtr<SWidget>& Widget) const;
	void ClearLog() const;

private:
	TAttribute<FText> NumSamplesText;
	TAttribute<FText> AvgSourceErrorText;
	TAttribute<FText> AvgDestErrorText;

	FReprojectionCalibrationButtonClicked OnCaptureClicked;
	FReprojectionCalibrationButtonClicked OnRestartClicked;
	FReprojectionCalibrationButtonClicked OnResetClicked;

	TSharedPtr<SScrollBox> ScrollBox;
};
