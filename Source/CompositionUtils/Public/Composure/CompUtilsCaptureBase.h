#pragma once

#include "CompositingCaptureBase.h"

#include "SlCompCaptureBase.generated.h"


struct FVolumetricFogRequiredDataProxy;

struct FStereolabsCameraTexturesProxy
{
	UTexture* ColorTexture   = nullptr;
	UTexture* DepthTexture   = nullptr;
	UTexture* NormalsTexture = nullptr;
};


/**
 *	Base class for CG Compositing elements that will work with Stereolabs Cameras
 */
UCLASS(BlueprintType)
class STEREOLABSCOMPOSITING_API AStereolabsCompositingCaptureBase : public ACompositingCaptureBase
{
	GENERATED_BODY()

public:
	AStereolabsCompositingCaptureBase();


	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="Composure|Injection")
	bool bInjectionMode = false;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="Composure|Injection", meta=(EditCondition="bInjectionMode"))
	FName CameraColorPassName;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="Composure|Injection", meta=(EditCondition="bInjectionMode"))
	FName CameraDepthPassName;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="Composure|Injection", meta=(EditCondition="bInjectionMode"))
	FName CameraNormalsPassName;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Composure|Injection", meta = (EditCondition = "bInjectionMode"))
	bool bDisableReprojectionUVMap = false;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="Composure|Injection", meta=(EditCondition="bInjectionMode"))
	bool bExtractVolumetricFogInInjectionMode = false;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="Composure|Injection|Lighting", meta=(EditCondition="bInjectionMode", ClampMin="0.0"))
	float AlbedoMultiplier = 1.0f;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="Composure|Injection|Lighting", meta=(EditCondition="bInjectionMode", ClampMin = "0.0"))
	float AmbientMultiplier = 1.0f;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="Composure|Injection|Lighting", meta=(EditCondition="bInjectionMode", ClampMin = "0.0", ClampMax = "1.0"))
	float RoughnessOverride = 1.0f;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="Composure|Injection|Lighting", meta=(EditCondition="bInjectionMode", ClampMin = "0.0", ClampMax = "1.0"))
	float SpecularOverride = 0.0f;

public:

	// Only dereference on render thread!
	// TODO: Return reference type instead of pointer type - the struct itself will always exist
	const FVolumetricFogRequiredDataProxy* GetVolumetricFogData() const;
protected:
	FVolumetricFogRequiredDataProxy* GetVolumetricFogData();

public:
	// Call this in GenerateInputs before rendering the scene capture
	UFUNCTION(BlueprintCallable, Category="Composure|Stereolabs Compositing", CallInEditor)
	void FetchLatestCameraTextures_GameThread();

	const FStereolabsCameraTexturesProxy& GetCameraTextures_RenderThread() const;

protected:
	// Rendering resources extracted from the scene renderer for use in composition
	// This layer provides a place to keep these resources safe and reference them in later Composure passes,
	// after the scene rendering has been completed
	TSharedPtr<struct FVolumetricFogRequiredDataProxy> VolumetricFogData_RenderThread;

	FStereolabsCameraTexturesProxy CameraTextures_RenderThread;

private:
	TSharedPtr<class FSlCompViewExtension, ESPMode::ThreadSafe> SlCompViewExtension;

	friend class FSlCompViewExtension;
};
