// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "ReprojectionCalibrationData.generated.h"

/**
 * Represents calibrated data to be able to reproject from a source camera to a target camera
 */
UCLASS()
class COMPOSITIONUTILS_API UReprojectionCalibrationData : public UDataAsset
{
	GENERATED_BODY()
public:
	UReprojectionCalibrationData();

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FTransform ExtrinsicTransform;
};
