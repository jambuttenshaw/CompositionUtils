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
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

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

	TSharedPtr<FSlateImageBrush> SourceBrush;
	TSharedPtr<FSlateImageBrush> DestinationBrush;
};
