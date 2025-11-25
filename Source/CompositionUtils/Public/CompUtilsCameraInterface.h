// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"

#include "CompUtilsCameraData.h"

#include "CompUtilsCameraInterface.generated.h"

UINTERFACE(MinimalAPI)
class UCompUtilsCameraInterface : public UInterface
{
	GENERATED_BODY()
};

/**
 * Represents an object that has the properties of a camera to CompositionUtils systems
 * A camera has intrinsic properties: Field of view, focal length, etc.
 */
class COMPOSITIONUTILS_API ICompUtilsCameraInterface
{
	GENERATED_BODY()
public:

	virtual bool GetCameraIntrinsicData(FCompUtilsCameraIntrinsicData& OutData);

};
