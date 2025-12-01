#include "Widgets/SReprojectionCalibrationControls.h"

#define LOCTEXT_NAMESPACE "FCompositionUtilsEditorModule"


void SReprojectionCalibrationControls::Construct(const FArguments& InArgs)
{
	OnCaptureImagePressed = InArgs._OnCaptureImagePressed;
	OnResetCalibrationPressed = InArgs._OnResetCalibrationPressed;

	ChildSlot
	[
		SNew(SVerticalBox)
		+ SVerticalBox::Slot()
		.HAlign(HAlign_Left)
		.VAlign(VAlign_Top)
		[
			SNew(SButton)
			.Text(LOCTEXT("CaptureImage", "Capture Image"))
			.OnClicked_Lambda([this]()
			{
				this->OnCaptureImagePressed.ExecuteIfBound();
				return FReply::Handled();
			})
		]
		+ SVerticalBox::Slot()
		.HAlign(HAlign_Left)
		.VAlign(VAlign_Top)
		[
			SNew(SButton)
			.Text(LOCTEXT("ResetCalibration", "Reset Calibration"))
			.OnClicked_Lambda([this]()
			{
				this->OnResetCalibrationPressed.ExecuteIfBound();
				return FReply::Handled();
			})
		]
	];
}

#undef LOCTEXT_NAMESPACE