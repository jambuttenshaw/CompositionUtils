#pragma once

#include "CoreMinimal.h"

#include "CompUtilsCameraData.generated.h"


UENUM(BlueprintType)
enum class ECompUtilsCameraType : uint8
{
	CameraType_Unknown=0		UMETA(DisplayName="Unkown"),
	CameraType_Virtual			UMETA(DisplayName="Virtual"),
	CameraType_Physical			UMETA(DisplayName="Physical"),
	// TODO: Distinguish between physical cameras that have colour only and physical cameras providing depth?
};

/**
 * All the data that is required to be known about a camera to involve it in composition / reprojection
 * This could be a real or virtual camera
 */
USTRUCT(BlueprintType)
struct COMPOSITIONUTILS_API FCompUtilsCameraIntrinsicData
{
	GENERATED_BODY()

	ECompUtilsCameraType Type = ECompUtilsCameraType::CameraType_Unknown;

	FMatrix44f ViewToNDC = FMatrix44f::Identity;	// Projection Matrix
	FMatrix44f NDCToView = FMatrix44f::Identity;	// Inv Projection Matrix

	// FOV is measured in radians
	float HorizontalFOV = HALF_PI;
	float VerticalFOV	= HALF_PI;
};
