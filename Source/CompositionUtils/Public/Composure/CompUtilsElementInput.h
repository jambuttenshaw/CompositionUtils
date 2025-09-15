#pragma once

#include "CoreMinimal.h"
#include "Composure/Classes/CompositingElements/CompositingElementPasses.h"

#include "CompUtilsElementInput.generated.h"


UCLASS(BlueprintType, Blueprintable)
class COMPOSITIONUTILS_API UCompositionUtilsAuxiliaryCameraInput : public UCompositingElementInput
{
	GENERATED_BODY()
public:

	// Begin Auxiliary Camera Interface
	virtual FMatrix44f GetProjectionMatrix() const { return FMatrix44f::Identity; }
	virtual FMatrix44f GetInverseProjectionMatrix() const { return FMatrix44f::Identity; }
	virtual float GetNearClippingPlane() const { return 10.0f; /* Sensible default */ };
	// End Auxiliary Camera Interface

};
