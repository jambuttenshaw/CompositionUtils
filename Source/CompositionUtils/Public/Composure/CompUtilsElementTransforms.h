#pragma once

#include "CoreMinimal.h"

#include "CompositingElements/CompositingElementPasses.h"
#include "Engine/DirectionalLight.h"

#include "SlCompElementTransforms.generated.h"


UCLASS(BlueprintType, Blueprintable)
class STEREOLABSCOMPOSITING_API UCompositingStereolabsDepthProcessingPass : public UCompositingElementTransform
{
	GENERATED_BODY()

public:

	// Reconstruction Parameters
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Compositing Pass", meta = (DisplayAfter = "PassName", InlineEditConditionToggle))
	bool bEnableJacobi = false;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Compositing Pass", meta = (DisplayAfter = "PassName", EditCondition = "bEnableJacobi"))
	int32 NumJacobiSteps = 10.0f;

	// Clipping Parameters
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Compositing Pass", meta = (DisplayAfter = "PassName", InlineEditConditionToggle))
	bool bEnableFarClipping = true;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Compositing Pass", meta = (DisplayAfter = "PassName", EditCondition = "bEnableFarClipping"))
	float FarClipDistance = 200.0f; // 200cm

	// Height of camera above floor
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Compositing Pass", meta = (DisplayAfter = "PassName", InlineEditConditionToggle))
	bool bEnableFloorClipping = true;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Compositing Pass", meta = (DisplayAfter = "PassName", EditCondition = "bEnableFloorClipping"))
	float FloorClipDistance = 100.0f; // 100cm

public:
	virtual UTexture* ApplyTransform_Implementation(UTexture* Input, UComposurePostProcessingPassProxy* PostProcessProxy, ACameraActor* TargetCamera) override;

};


/**
 * Composites volumetric fog from the scene onto the camera image, using the real-world depth
 */
UCLASS(BlueprintType, Blueprintable)
class STEREOLABSCOMPOSITING_API UCompositingStereolabsVolumetricsPass : public UCompositingElementTransform
{		
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Compositing Pass", meta = (DisplayAfter = "PassName", EditCondition = "bEnabled"))
	FName CameraDepthPassName;

	/** Used to get resources from the scene renderer that are required for composing volumetric effects */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Compositing Pass", meta = (DisplayAfter = "PassName", EditCondition = "bEnabled"))
	TWeakObjectPtr<class AStereolabsCompositingCaptureBase> StereolabsCGLayer;

public:
	virtual UTexture* ApplyTransform_Implementation(UTexture* Input, UComposurePostProcessingPassProxy* PostProcessProxy, ACameraActor* TargetCamera) override;

};


/**
 * Applies light sources from the virtual world to the camera image
 */
UCLASS(BlueprintType, Blueprintable)
class STEREOLABSCOMPOSITING_API UCompositingStereolabsRelightingPass : public UCompositingElementTransform
{
	GENERATED_BODY()

public:
	// Need the other camera inputs
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Compositing Pass", meta = (DisplayAfter = "PassName", EditCondition = "bEnabled"))
	FName CameraNormalPassName;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Compositing Pass", meta = (DisplayAfter = "PassName", EditCondition = "bEnabled"))
	FName CameraDepthPassName;

	// Light sources used for relighting
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Compositing Pass", meta = (DisplayAfter = "PassName", EditCondition = "bEnabled"))
	TWeakObjectPtr<ADirectionalLight> LightSource;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Compositing Pass", meta = (DisplayAfter = "PassName", EditCondition = "bEnabled"))
	float LightWeight = 1.0f;

public:
	virtual UTexture* ApplyTransform_Implementation(UTexture* Input, UComposurePostProcessingPassProxy* PostProcessProxy, ACameraActor* TargetCamera) override;

};


/**
 * To be able to preview the depth image directly in the preview window
 */
UCLASS(BlueprintType, Blueprintable)
class STEREOLABSCOMPOSITING_API UCompositingStereolabsDepthPreviewPass : public UCompositingElementTransform
{
	GENERATED_BODY()

public:

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Compositing Pass", meta = (DisplayAfter = "PassName", EditCondition = "bEnabled", ClampMin="0"))
	FVector2D VisualizeDepthRange = { 0, 1000 };

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Compositing Pass", meta = (DisplayAfter = "PassName", EditCondition = "bEnabled"))
	bool bVisualizeReprojectionUVMap = true;

public:
	virtual UTexture* ApplyTransform_Implementation(UTexture* Input, UComposurePostProcessingPassProxy* PostProcessProxy, ACameraActor* TargetCamera) override;

private:

	void ApplyVisualizeDepth(UTexture* Input, UTextureRenderTarget2D* RenderTarget) const;
	void ApplyVisualizeReprojectionUVMap(UTexture* Input, UTextureRenderTarget2D* RenderTarget, ACameraActor* TargetCamera) const;

};


/**
 *	For easier previewing and interpretation of the normal map in the composure preview window
 */
UCLASS(BlueprintType, Blueprintable)
class STEREOLABSCOMPOSITING_API UCompositionStereolabsNormalMapPreviewPass : public UCompositingElementTransform
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Compositing Pass", meta = (DisplayAfter = "PassName", EditCondition = "bEnabled"))
	bool bDisplayWorldSpaceNormals = true;

public:

public:
	virtual UTexture* ApplyTransform_Implementation(UTexture* Input, UComposurePostProcessingPassProxy* PostProcessProxy, ACameraActor* TargetCamera) override;

};
