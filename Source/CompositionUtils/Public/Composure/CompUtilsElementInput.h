#pragma once

#include "CoreMinimal.h"
#include "CompositingElement.h"
#include "Composure/Classes/CompositingElements/CompositingElementPasses.h"

#include "CompUtilsElementInput.generated.h"


USTRUCT(BlueprintType)
struct FAuxiliaryCameraData
{
	GENERATED_BODY()

	FMatrix44f ViewToNDCMatrix = FMatrix44f::Identity;	// Projection Matrix
	FMatrix44f NDCToViewMatrix = FMatrix44f::Identity;	// Inv Projection Matrix

	float NearClipPlane = 1.0f;							// In practice should be the minimum depth of the physical camera,
														// but so long as it is consistent throughout compositing pipeline it should work regardless
	float HorizontalFOV = 90.0f;
	float VerticalFOV = 90.0f;
};


UCLASS(BlueprintType, Blueprintable)
class COMPOSITIONUTILS_API UCompositionUtilsAuxiliaryCameraInput : public UCompositingElementInput
{
	GENERATED_BODY()
public:

	// Begin Auxiliary Camera Interface
	virtual bool GetCameraData(FAuxiliaryCameraData& OutData) { return false; }
	// End Auxiliary Camera Interface


public:
	// Helper function to attempt to acquire this input pass from a compositing element
	// The weak ptr will be invalid if the attempt failed
	static TWeakObjectPtr<UCompositionUtilsAuxiliaryCameraInput> TryGetAuxCameraInputPassFromCompositingElement(const TWeakObjectPtr<ACompositingElement>& CompositingElement);
};
