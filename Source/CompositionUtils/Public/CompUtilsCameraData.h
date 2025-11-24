#pragma once

#include "CoreMinimal.h"

#include "CompUtilsCameraData.generated.h"


/**
 * All the data that is required to be known about a camera to involve it in composition / reprojection
 * This could be a real or virtual camera
 */
USTRUCT(BlueprintType)
struct COMPOSITIONUTILS_API FCompUtilsCameraIntrinsicData
{
	GENERATED_BODY()

	FMatrix44f ViewToNDCMatrix = FMatrix44f::Identity;	// Projection Matrix
	FMatrix44f NDCToViewMatrix = FMatrix44f::Identity;	// Inv Projection Matrix

	float HorizontalFOV = 90.0f;
	float VerticalFOV = 90.0f;
};
