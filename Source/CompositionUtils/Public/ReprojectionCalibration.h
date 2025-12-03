// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "CompUtilsCameraInterface.h"

#include "ReprojectionCalibration.generated.h"


/**
 * Composition Utils: Base class for encapsulating a target for reprojection (either as a source or destination)
 * Implements ICompUtilsCameraInterface, as a reprojection target must expose intrinsic properties of a camera
 */
UCLASS(editinlinenew, Abstract)
class COMPOSITIONUTILS_API UReprojectionCalibrationTargetBase : public UObject, public ICompUtilsCameraInterface
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

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Calibration", DisplayName = "Num Checkerboard Vertices", 
		meta = (ToolTip = "The number of inisde corners on each axis of the checkerboard. E.g. a conventional chessboard is 7x7."))
	FIntPoint CheckerboardDimensions;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Calibration", DisplayName = "Checkerboard Size (cm)",
		meta = (ToolTip = "The side length of each square in the checkerboard, measured in centimetres. E.g. a checkerboard with squares 25mm in side length would be 2.5."))
	float CheckerboardSize;
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
