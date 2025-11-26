#pragma once

#include "CoreMinimal.h"

class COMPOSITIONUTILSEDITOR_API SReprojectionCalibrationWidget : public SLeafWidget
{
public:
	SLATE_BEGIN_ARGS(SReprojectionCalibrationWidget)
		{}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
};
