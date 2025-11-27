// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "ReprojectionCalibration.generated.h"

/**
 * Composition Utils: Contains calibrated data to enable reprojection from a source camera to a target camera
 */
UCLASS()
class COMPOSITIONUTILS_API UReprojectionCalibration : public UDataAsset
{
	GENERATED_BODY()
public:
	UReprojectionCalibration();

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FTransform ExtrinsicTransform;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TObjectPtr<AActor> Source;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TObjectPtr<AActor> Target;
};
