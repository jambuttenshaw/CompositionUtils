#pragma once

#include "CoreMinimal.h"

DECLARE_DELEGATE(FTickTargetTexture)

class COMPOSITIONUTILSEDITOR_API SReprojectionCalibrationViewWidget : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SReprojectionCalibrationViewWidget)
		{}
		SLATE_ATTRIBUTE(TObjectPtr<UTexture>, SourceTexture)
		SLATE_ATTRIBUTE(TObjectPtr<UTexture>, DestinationTexture)

		SLATE_EVENT(FTickTargetTexture, TickSourceTexture)
		SLATE_EVENT(FTickTargetTexture, TickDestinationTexture)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;

	void InvalidateBrushes();

private:
	const FSlateBrush* GetSourceImageBrush();
	const FSlateBrush* GetDestinationImageBrush();

	// Tries to create a brush from the image stored in attribute Image
	// If it succeeds, it will store the created brush in OutBrush
	// If it fails, it will return the fallback brush
	const FSlateBrush* TryCreateNewBrush(const TAttribute<TObjectPtr<UTexture>>& Image, TSharedPtr<FSlateImageBrush>& OutBrush, const FSlateBrush* Fallback);

private:
	const FSlateBrush* FallbackBrush = nullptr;

	TAttribute<TObjectPtr<UTexture>> SourceTexture;
	TAttribute<TObjectPtr<UTexture>> DestinationTexture;

	FTickTargetTexture TickSourceTexture;
	FTickTargetTexture TickDestinationTexture;

	TSharedPtr<FSlateImageBrush> SourceBrush;
	TSharedPtr<FSlateImageBrush> DestinationBrush;
};
