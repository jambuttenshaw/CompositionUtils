#include "SReprojectionCalibrationViewWidget.h"

#include "Widgets/Layout/SScaleBox.h"


void SReprojectionCalibrationViewWidget::Construct(const FArguments& InArgs)
{
	SourceTexture = InArgs._SourceTexture;
	DestinationTexture = InArgs._DestinationTexture;

	TickSourceTexture = InArgs._TickSourceTexture;
	TickDestinationTexture = InArgs._TickDestinationTexture;

	FallbackBrush = FCoreStyle::Get().GetBrush("Checkerboard");

	ChildSlot
	[
		SNew(SScaleBox)
			.Stretch(EStretch::ScaleToFit)
			.StretchDirection(EStretchDirection::Both)
			.HAlign(HAlign_Fill)
			.VAlign(VAlign_Top)
			[
				SNew(SHorizontalBox)
					+ SHorizontalBox::Slot()
					.FillWidth(1.0f)
					.HAlign(HAlign_Fill)
					.VAlign(VAlign_Fill)
					[
						SNew(SScaleBox)
							.Stretch(EStretch::ScaleToFit)
							.StretchDirection(EStretchDirection::Both)
							.HAlign(HAlign_Center)
							.VAlign(VAlign_Center)
							[
								SNew(SImage)
									.Image_Lambda([this](){ return this->GetSourceImageBrush(); })
							]
					]
					+ SHorizontalBox::Slot()
					.FillWidth(1.0f)
					.HAlign(HAlign_Fill)
					.VAlign(VAlign_Fill)
					[
						SNew(SScaleBox)
							.Stretch(EStretch::ScaleToFit)
							.StretchDirection(EStretchDirection::Both)
							.HAlign(HAlign_Center)
							.VAlign(VAlign_Center)
							[
								SNew(SImage)
									.Image_Lambda([this]() { return this->GetDestinationImageBrush(); })
							]
					]
			]
	];
}

void SReprojectionCalibrationViewWidget::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	SCompoundWidget::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);

	TickSourceTexture.ExecuteIfBound();
	TickDestinationTexture.ExecuteIfBound();
}

void SReprojectionCalibrationViewWidget::InvalidateBrushes()
{
	SourceBrush.Reset();
	DestinationBrush.Reset();
}

const FSlateBrush* SReprojectionCalibrationViewWidget::GetSourceImageBrush()
{
	if (SourceBrush.IsValid())
		return SourceBrush.Get();

	return TryCreateNewBrush(SourceTexture, SourceBrush, FallbackBrush);
}

const FSlateBrush* SReprojectionCalibrationViewWidget::GetDestinationImageBrush()
{
	if (DestinationBrush.IsValid())
		return DestinationBrush.Get();
		
	return TryCreateNewBrush(DestinationTexture, DestinationBrush, FallbackBrush);
}

const FSlateBrush* SReprojectionCalibrationViewWidget::TryCreateNewBrush(const TAttribute<TObjectPtr<UTexture>>& Image, TSharedPtr<FSlateImageBrush>& OutBrush, const FSlateBrush* Fallback)
{
	if (Image.IsSet())
	{
		if (auto Texture = Image.Get())
		{
			const FVector2f Size(16, 9);
			OutBrush = MakeShared<FSlateImageBrush>(Texture, Size, FLinearColor::White, ESlateBrushTileType::NoTile);
			return OutBrush.Get();
		}
	}

	return Fallback;
}
