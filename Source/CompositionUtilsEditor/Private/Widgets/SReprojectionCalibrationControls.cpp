#include "Widgets/SReprojectionCalibrationControls.h"

#define LOCTEXT_NAMESPACE "FCompositionUtilsEditorModule"


void SReprojectionCalibrationControls::Construct(const FArguments& InArgs)
{
	OnCaptureClicked = InArgs._OnCaptureClicked;
	OnRestartClicked = InArgs._OnRestartClicked;
	OnResetClicked = InArgs._OnResetClicked;

	NumSamplesText = InArgs._GetNumRunsText;
	AvgSourceErrorText = InArgs._GetAvgSourceErrorText;
	AvgDestErrorText = InArgs._GetAvgDestErrorText;

	ChildSlot
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.HAlign(HAlign_Fill)
		.VAlign(VAlign_Top)
		.AutoWidth()
		.Padding(20.0f, 10.0f)
		[
			SNew(SBox)
			.MinDesiredWidth(300.0f)
			.HAlign(HAlign_Fill)
			.VAlign(VAlign_Fill)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot()
				.HAlign(HAlign_Fill)
				.VAlign(VAlign_Center)
				.FillHeight(1.0f)
				.Padding(10.0f)
				[
					SNew(SButton)
					.Text(LOCTEXT("CaptureImage", "Capture Image"))
					.HAlign(HAlign_Center)
					.ContentPadding(10.0f)
					.OnClicked_Lambda([this]()
					{
						this->OnCaptureClicked.ExecuteIfBound();
						return FReply::Handled();
					})
					.ButtonStyle(FAppStyle::Get(), "EditorUtilityButton")
				]
				+ SVerticalBox::Slot()
				.HAlign(HAlign_Fill)
				.VAlign(VAlign_Center)
				.FillHeight(1.0f)
				.Padding(10.0f)
				[
					SNew(SButton)
					.Text(LOCTEXT("RestartCalibration", "Restart Calibration"))
					.HAlign(HAlign_Center)
					.ContentPadding(10.0f)
					.OnClicked_Lambda([this]()
					{
						this->OnRestartClicked.ExecuteIfBound();
						return FReply::Handled();
					})
					.ButtonStyle(FAppStyle::Get(), "EditorUtilityButton")
				]
				+ SVerticalBox::Slot()
				.HAlign(HAlign_Fill)
				.VAlign(VAlign_Center)
				.FillHeight(1.0f)
				.Padding(10.0f)
				[
					SNew(SButton)
					.Text(LOCTEXT("ResetCalibration", "Reset Calibration"))
					.HAlign(HAlign_Center)
					.ContentPadding(10.0f)
					.OnClicked_Lambda([this]()
					{
						this->OnResetClicked.ExecuteIfBound();
						return FReply::Handled();
					})
					.ButtonStyle(FAppStyle::Get(), "EditorUtilityButton")
				]
			]
		]
		+ SHorizontalBox::Slot()
		.HAlign(HAlign_Fill)
		.VAlign(VAlign_Fill)
		.AutoWidth()
		.Padding(20.0f, 10.0f)
		[
			SNew(SGridPanel)
			+ SGridPanel::Slot(0, 0)
			.HAlign(HAlign_Left)
			.VAlign(VAlign_Fill)
			.Padding(10.0f)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("NumSamplesLabel", "Num Successful Samples:"))
			]
			+ SGridPanel::Slot(1, 0)
			.HAlign(HAlign_Right)
			.VAlign(VAlign_Fill)
			.Padding(10.0f)
			[
				SNew(STextBlock)
					.Text(NumSamplesText)
			]
			+ SGridPanel::Slot(0, 1)
			.HAlign(HAlign_Left)
			.VAlign(VAlign_Fill)
			.Padding(10.0f)
			[
				SNew(STextBlock)
					.Text(LOCTEXT("AvgSrcErrorLabel", "Average Error (Source):"))
			]
			+ SGridPanel::Slot(1, 1)
			.HAlign(HAlign_Right)
			.VAlign(VAlign_Fill)
			.Padding(10.0f)
			[
				SNew(STextBlock)
					.Text(AvgSourceErrorText)
			]
			+ SGridPanel::Slot(0, 2)
			.HAlign(HAlign_Left)
			.VAlign(VAlign_Fill)
			.Padding(10.0f)
			[
				SNew(STextBlock)
					.Text(LOCTEXT("AvgDestErrorLabel", "Average Error (Destination):"))
			]
			+ SGridPanel::Slot(1, 2)
			.HAlign(HAlign_Right)
			.VAlign(VAlign_Fill)
			.Padding(10.0f)
			[
				SNew(STextBlock)
					.Text(AvgDestErrorText)
			]
		]
		+ SHorizontalBox::Slot()
		.HAlign(HAlign_Fill)
		.VAlign(VAlign_Fill)
		.FillWidth(1.0f)
		.Padding(20.0f, 10.0f)
		[
			SAssignNew(ScrollBox, SScrollBox)
		]
	];
}

void SReprojectionCalibrationControls::AddSuccessToLog(int32 SampleIndex, double SourceError, double DestError, const FTransform& CalibratedTransform, double Weight) const
{
	FVector Translation = CalibratedTransform.GetTranslation();
	FVector Rotation = CalibratedTransform.GetRotation().Euler();

	FNumberFormattingOptions Options;
	Options
		.SetMaximumFractionalDigits(2);

	AddToLog(
		SNew(STextBlock)
		.Text(FText::Format(LOCTEXT("SuccessFormat",
			"Calibration sample {0} succeeded:\n"
			"	Error (Source): {1}\n"
			"	Error (Destination): {2}\n"
			"	Translation: ({3}, {4}, {5})\n"
			"	Rotation: ({6}, {7}, {8})\n"
			"	Weight: {9}\n"
			),
			FText::AsNumber(SampleIndex, &Options),
			FText::AsNumber(SourceError, &Options),
			FText::AsNumber(DestError, &Options),
			FText::AsNumber(Translation.X, &Options), FText::AsNumber(Translation.Y, &Options), FText::AsNumber(Translation.Z, &Options),
			FText::AsNumber(Rotation.X, &Options), FText::AsNumber(Rotation.Y, &Options), FText::AsNumber(Rotation.Z, &Options),
			FText::AsNumber(Weight, &Options)
			))
	);
}

void SReprojectionCalibrationControls::AddErrorToLog(const FText& ErrorText) const
{
	AddToLog(
		SNew(STextBlock)
		.Text(FText::Format(LOCTEXT("ErrorFormat", 
		"Calibration failed with error:\n"
		"	{0}"),
		ErrorText))
		.ColorAndOpacity(FColor::Red)
	);
}

void SReprojectionCalibrationControls::AddToLog(const TSharedPtr<SWidget>& Widget) const
{
	// Display most recent log first
	ScrollBox->InsertSlot(0)
	.HAlign(HAlign_Fill)
	.VAlign(VAlign_Fill)
	.AutoSize()
	.Padding(10.0f)
	[
		Widget.ToSharedRef()
	];
}

void SReprojectionCalibrationControls::ClearLog() const
{
	ScrollBox->ClearChildren();
}

#undef LOCTEXT_NAMESPACE
