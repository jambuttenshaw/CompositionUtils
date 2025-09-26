#pragma once

#include "CoreMinimal.h"

#include "CompositingElement.h"
#include "CompUtilsElementInput.h"
#include "CompositingElements/CompositingElementPasses.h"
#include "Engine/DirectionalLight.h"

#include "CompUtilsElementTransforms.generated.h"


class FRHIGPUBufferReadback;

UCLASS(BlueprintType, Blueprintable)
class COMPOSITIONUTILS_API UCompositionUtilsDepthProcessingPass : public UCompositingElementTransform
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

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Compositing Pass", meta = (DisplayAfter = "PassName"))
	TWeakObjectPtr<ACompositingElement> AuxiliaryCameraInputElement;

public:
	//~ Begin UCompositingElementTransform interface
	virtual UTexture* ApplyTransform_Implementation(UTexture* Input, UComposurePostProcessingPassProxy* PostProcessProxy, ACameraActor* TargetCamera) override;
	//~ End UCompositingElementTransform interface

	//~ Begin UObject interface
#if WITH_EDITOR
	virtual void PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
	//~ End UObject interface

private:
	TWeakObjectPtr<UCompositionUtilsAuxiliaryCameraInput> AuxiliaryCameraInput;
};


/**
 * Aligns the depth image (input to this pass) as if it had been taken from the POV of a different camera, specified by the Virtual Camera Target Actor
 */
UCLASS(BlueprintType, Blueprintable)
class COMPOSITIONUTILS_API UCompositionUtilsDepthAlignmentPass : public UCompositingElementTransform
{
	GENERATED_BODY()

public:

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Compositing Pass|Setup", meta = (DisplayAfter = "PassName", EditCondition = "bEnabled"))
	TWeakObjectPtr<ACompositingElement> AuxiliaryCameraInputElement;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Compositing Pass|Setup", meta = (DisplayAfter = "PassName", EditCondition = "bEnabled"))
	TWeakObjectPtr<ACameraActor> VirtualCameraTargetActor;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Compositing Pass", meta = (DisplayAfter = "PassName", EditCondition = "bEnabled"))
	FTransform AuxiliaryToPrimaryNodalOffset;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Compositing Pass", meta = (DisplayAfter = "PassName", EditCondition = "bEnabled", ClampMin="0", ClampMax="8"))
	int32 HoleFillingBias = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Compositing Pass|Calibration", meta = (EditCondition = "bEnabled"))
	bool bShowCalibrationPoints = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Compositing Pass|Calibration", meta = (EditCondition = "bEnabled"))
	bool bRunCalibration = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Compositing Pass|Calibration", meta = (EditCondition = "bEnabled"))
	bool bImmediatelyResetCalibrationFlag = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Compositing Pass|Calibration", meta = (EditCondition = "bEnabled&&bShowCalibrationPoints"))
	int32 CalibrationPointCount = 32;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Compositing Pass|Calibration", 
				meta = (EditCondition = "bEnabled&&bShowCalibrationPoints", ClampMin = "0", ClampMax = "1"))
	FVector2f InterestPointSpawnMin{ 0.0f };

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Compositing Pass|Calibration", 
				meta = (EditCondition = "bEnabled&&bShowCalibrationPoints", ClampMin = "0", ClampMax = "1"))
	FVector2f InterestPointSpawnMax{ 1.0f };

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Compositing Pass|Calibration", meta = (EditCondition = "bEnabled"))
	float KnownDistance = 200.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Compositing Pass|Calibration", meta = (EditCondition = "bEnabled", ClampMin="-180", ClampMax="180"))
	float TangentAlignmentAngle = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Compositing Pass|Calibration", meta = (EditCondition = "bEnabled"))
	FVector2D AlignmentOffset;

public:
	//~ Begin UCompositingElementTransform interface
	virtual UTexture* ApplyTransform_Implementation(UTexture* Input, UComposurePostProcessingPassProxy* PostProcessProxy, ACameraActor* TargetCamera) override;
	//~ End UCompositingElementTransform interface

	//~ Begin UObject interface
#if WITH_EDITOR
	virtual void PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
	//~ End UObject interface

private:
	// Reads data back from GPU to perform calibration
	void CalibrateAlignment_RenderThread(FRHIGPUBufferReadback& Readback);
	void UpdateCalibratedAlignment_GameThread(const FTransform& CalibratedTransform);

private:
	TWeakObjectPtr<UCompositionUtilsAuxiliaryCameraInput> AuxiliaryCameraInput;
};


/**
 * Composites volumetric fog from the scene onto the camera image, using the real-world depth
 */
UCLASS(BlueprintType, Blueprintable)
class COMPOSITIONUTILS_API UCompositionUtilsVolumetricsPass : public UCompositingElementTransform
{		
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Compositing Pass", meta = (DisplayAfter = "PassName", EditCondition = "bEnabled"))
	FName CameraDepthPassName;

	/** Used to get resources from the scene renderer that are required for composing volumetric effects */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Compositing Pass", meta = (DisplayAfter = "PassName", EditCondition = "bEnabled"))
	TWeakObjectPtr<class ACompositionUtilsCaptureBase> CompUtilsCGLayer;

public:
	virtual UTexture* ApplyTransform_Implementation(UTexture* Input, UComposurePostProcessingPassProxy* PostProcessProxy, ACameraActor* TargetCamera) override;

};


/**
 * Applies light sources from the virtual world to the camera image
 */
UCLASS(BlueprintType, Blueprintable)
class COMPOSITIONUTILS_API UCompositionUtilsRelightingPass : public UCompositingElementTransform
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
 * Programmatically adds a crosshair to an image to assist with calibration and alignment.
 */
UCLASS(BlueprintType, Blueprintable)
class COMPOSITIONUTILS_API UCompositionUtilsAddCrosshairPass : public UCompositingElementTransform
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Compositing Pass", meta = (DisplayAfter = "PassName", EditCondition = "bEnabled"))
	FLinearColor Color{ 1.0f, 1.0f, 0.0f };

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Compositing Pass", meta = (DisplayAfter = "PassName", EditCondition = "bEnabled", ClampMin="0"))
	int32 Width = 4;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Compositing Pass", meta = (DisplayAfter = "PassName", EditCondition = "bEnabled", ClampMin="0"))
	int32 Length = 50;

public:
	virtual UTexture* ApplyTransform_Implementation(UTexture* Input, UComposurePostProcessingPassProxy* PostProcessProxy, ACameraActor* TargetCamera) override;
};


/**
 * To be able to preview the depth image directly in the preview window
 */
UCLASS(BlueprintType, Blueprintable)
class COMPOSITIONUTILS_API UCompositionUtilsDepthPreviewPass : public UCompositingElementTransform
{
	GENERATED_BODY()

public:

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Compositing Pass", meta = (DisplayAfter = "PassName", EditCondition = "bEnabled", ClampMin="0"))
	FVector2D VisualizeDepthRange = { 0, 1000 };

public:
	virtual UTexture* ApplyTransform_Implementation(UTexture* Input, UComposurePostProcessingPassProxy* PostProcessProxy, ACameraActor* TargetCamera) override;

private:

	void ApplyVisualizeDepth(UTexture* Input, UTextureRenderTarget2D* RenderTarget) const;
};


/**
 *	For easier previewing and interpretation of the normal map in the composure preview window
 */
UCLASS(BlueprintType, Blueprintable)
class COMPOSITIONUTILS_API UCompositionUtilsNormalMapPreviewPass : public UCompositingElementTransform
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Compositing Pass", meta = (DisplayAfter = "PassName", EditCondition = "bEnabled"))
	bool bDisplayWorldSpaceNormals = true;

public:

public:
	virtual UTexture* ApplyTransform_Implementation(UTexture* Input, UComposurePostProcessingPassProxy* PostProcessProxy, ACameraActor* TargetCamera) override;

};
