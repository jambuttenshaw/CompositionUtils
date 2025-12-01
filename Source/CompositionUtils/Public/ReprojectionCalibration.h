// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "ReprojectionCalibration.generated.h"


/**
 * Composition Utils: Base class for encapsulating a target for reprojection (either as a source or destination)
 */
UCLASS(editinlinenew, Abstract)
class COMPOSITIONUTILS_API UReprojectionCalibrationTargetBase : public UObject
{
	GENERATED_BODY()
public:

	// Interface that allows derived classes to provide their own targets
	virtual TObjectPtr<UTexture> GetTexture() { return nullptr; }
};


/**
 * Composition Utils: Contains calibrated data to enable reprojection from a source camera to a destination camera
 */
UCLASS()
class COMPOSITIONUTILS_API UReprojectionCalibration : public UObject
{
	GENERATED_BODY()
public:
	UReprojectionCalibration();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Calibrated Data")
	FTransform ExtrinsicTransform;

	UPROPERTY(EditAnywhere, Instanced, BlueprintReadOnly, Category = "Calibration", meta = (ShowOnlyInnerProperties))
	TObjectPtr<UReprojectionCalibrationTargetBase> Source;

	UPROPERTY(EditAnywhere, Instanced, BlueprintReadOnly, Category = "Calibration", meta = (ShowOnlyInnerProperties))
	TObjectPtr<UReprojectionCalibrationTargetBase> Destination;
};


/**
 * Composition Utils: Represents a media texture as a target for reprojection
 */
UCLASS()
class COMPOSITIONUTILS_API UReprojectionCalibrationMediaTarget : public UReprojectionCalibrationTargetBase
{
	GENERATED_BODY()
public:

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TObjectPtr<class UMediaTexture> MediaTexture;

public:

	//~ Begin UReprojectionCalibrationTarget Interface
	virtual TObjectPtr<UTexture> GetTexture() override;
	//~ End UReprojectionCalibrationTarget Interface
};
