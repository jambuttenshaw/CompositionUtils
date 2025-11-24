#pragma once

#include "CompositingCaptureBase.h"
#include "CompUtilsElementInput.h"

#include "CompUtilsCaptureBase.generated.h"


struct FVolumetricFogRequiredDataProxy;

struct FCameraTexturesProxy
{
	UTexture* ColorTexture   = nullptr;
	UTexture* DepthTexture   = nullptr;
	UTexture* NormalsTexture = nullptr;
};


/**
 *	Base class for CG Compositing elements that will work with depth cameras
 */
UCLASS(BlueprintType)
class COMPOSITIONUTILS_API ACompositionUtilsCaptureBase : public ACompositingCaptureBase
{
	GENERATED_BODY()

public:
	ACompositionUtilsCaptureBase();


	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="Composure|Injection")
	bool bInjectionMode = false;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Composure|Injection", meta = (EditCondition = "bInjectionMode", DisplayAfter = "bInjectionMode"))
	bool bUseOverrideColorPass = false;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Composure|Injection", meta = (EditCondition = "bInjectionMode", DisplayAfter = "bUseOverrideColorPass"))
	bool bAlignColorAndNormals = false;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="Composure|Injection", meta=(EditCondition="bInjectionMode&&!bUseOverrideColorPass"))
	FName CameraColorPassName;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="Composure|Injection", meta=(EditCondition="bInjectionMode&&bUseOverrideColorPass"))
	FName OverrideCameraColorPassName;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="Composure|Injection", meta=(EditCondition="bInjectionMode"))
	FName CameraDepthPassName;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="Composure|Injection", meta=(EditCondition="bInjectionMode"))
	FName CameraNormalsPassName;

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

private:
	TWeakObjectPtr<UCompositionUtilsCameraInput> AuxiliaryCameraInput;

public:
	// Call this in GenerateInputs before rendering the scene capture
	UFUNCTION(BlueprintCallable, Category="Composure|Compositing Utils", CallInEditor)
	void FetchLatestCameraTextures_GameThread();

	const FCameraTexturesProxy& GetCameraTextures_RenderThread() const;

protected:
	// Rendering resources extracted from the scene renderer for use in composition
	// This layer provides a place to keep these resources safe and reference them in later Composure passes,
	// after the scene rendering has been completed
	TSharedPtr<struct FVolumetricFogRequiredDataProxy> VolumetricFogData_RenderThread;

	FCameraTexturesProxy CameraTextures_RenderThread;

private:
	TSharedPtr<class FCompUtilsViewExtension, ESPMode::ThreadSafe> CompUtilsViewExtension;
	friend class FCompUtilsViewExtension;
};
