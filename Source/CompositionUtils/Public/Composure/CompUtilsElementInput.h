#pragma once

#include "CoreMinimal.h"

#include "CompositingElement.h"
#include "Composure/Classes/CompositingElements/CompositingElementPasses.h"

#include "CompUtilsCameraData.h"

#include "CompUtilsElementInput.generated.h"


UCLASS(BlueprintType, Blueprintable)
class COMPOSITIONUTILS_API UCompositionUtilsCameraInput : public UCompositingElementInput
{
	GENERATED_BODY()
public:

	//~ Begin Auxiliary Camera Interface
	virtual bool GetCameraIntrinsicData(FCompUtilsCameraIntrinsicData& OutData) { return false; }
	//~ End Auxiliary Camera Interface


public:
	// Helper function to attempt to acquire this input pass from a compositing element
	// The weak ptr will be invalid if the attempt failed
	static TWeakObjectPtr<UCompositionUtilsCameraInput> TryGetCameraInputPassFromCompositingElement(const TWeakObjectPtr<ACompositingElement>& CompositingElement);
};
