#include "Composure/CompUtilsElementTransforms.h"

#include "CompositionUtils.h"
#include "RenderGraphBuilder.h"
#include "RHIGPUReadback.h"
#include "TextureResource.h"

#include "CompositingElements/ICompositingTextureLookupTable.h"
#include "Composure/CompUtilsCaptureBase.h"
#include "Pipelines/CompUtilsPipelines.h"

#include "Components/DirectionalLightComponent.h"


//////////////////////////////////////////
// UCompositionUtilsDepthProcessingPass //
//////////////////////////////////////////

UTexture* UCompositionUtilsDepthProcessingPass::ApplyTransform_Implementation(UTexture* Input, UComposurePostProcessingPassProxy* PostProcessProxy, ACameraActor* TargetCamera)
{
	if (!Input)
		return Input;
	check(Input->GetResource());

	FIntPoint Dims;
	Dims.X = Input->GetResource()->GetSizeX();
	Dims.Y = Input->GetResource()->GetSizeY();

	UTextureRenderTarget2D* RenderTarget = RequestRenderTarget(Dims, PF_FloatRGBA);
	if (!(RenderTarget && RenderTarget->GetResource()))
		return Input;

	FDepthProcessingParametersProxy Params;

	if (!CameraInput.IsValid())
	{
		CameraInput = UCompositionUtilsCameraInput::TryGetCameraInputPassFromCompositingElement(AuxiliaryCameraInputElement);
	}

	FCompUtilsCameraIntrinsicData CameraIntrinsicData;
	if (CameraInput.IsValid() && CameraInput->GetCameraIntrinsicData(CameraIntrinsicData))
	{
		Params.InvProjectionMatrix = CameraIntrinsicData.NDCToViewMatrix;
	}
	else
	{
		Params.InvProjectionMatrix = FMatrix44f::Identity;
	}

	Params.bEnableJacobiSteps = bEnableJacobi;
	Params.NumJacobiSteps = NumJacobiSteps;

	Params.bEnableFarClipping = bEnableFarClipping;
	Params.FarClipDistance = FarClipDistance;

	Params.bEnableClippingPlane = bEnableFloorClipping;
	// Construct user clipping plane
	// THESE DIRECTIONS / POSITIONS USE Y AXIS AS UP/DOWN AND Z AXIS AS FRONT/BACK
	FPlane ClippingPlane{ FVector{ 0.0f, -FloorClipDistance, 0.0f }, FVector{ 0.0f, 1.0f, 0.0f } };
	Params.UserClippingPlane = FVector4f{
		static_cast<float>(ClippingPlane.X),
		static_cast<float>(ClippingPlane.Y),
		static_cast<float>(ClippingPlane.Z),
		static_cast<float>(ClippingPlane.W)
	};

	ENQUEUE_RENDER_COMMAND(ApplyDepthProcessingPass)(
		[Parameters = MoveTemp(Params), InputResource = Input->GetResource(), OutputResource = RenderTarget->GetResource()]
		(FRHICommandListImmediate& RHICmdList)
		{
			FRDGBuilder GraphBuilder(RHICmdList);

			TRefCountPtr<IPooledRenderTarget> InputRT = CreateRenderTarget(InputResource->GetTextureRHI(), TEXT("CompUtilsDepthProcessingPass.Input"));
			TRefCountPtr<IPooledRenderTarget> OutputRT = CreateRenderTarget(OutputResource->GetTextureRHI(), TEXT("CompUtilsDepthProcessingPass.Output"));

			// Set up RDG resources
			FRDGTextureRef InColorTexture = GraphBuilder.RegisterExternalTexture(InputRT);
			FRDGTextureRef OutColorTexture = GraphBuilder.RegisterExternalTexture(OutputRT);

			// Execute pipeline
			CompositionUtils::ExecuteDepthProcessingPipeline(
				GraphBuilder,
				Parameters,
				InColorTexture,
				OutColorTexture
			);

			GraphBuilder.Execute();
		});

	return RenderTarget;
}

#if WITH_EDITOR
void UCompositionUtilsDepthProcessingPass::PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent)
{
	const FName PropertyName = PropertyChangedEvent.GetPropertyName();
	if (PropertyName == GET_MEMBER_NAME_CHECKED(UCompositionUtilsDepthProcessingPass, AuxiliaryCameraInputElement))
	{
		CameraInput = nullptr;
	}

	Super::PostEditChangeProperty(PropertyChangedEvent);
}
#endif


/////////////////////////////////////////
// UCompositionUtilsDepthAlignmentPass //
/////////////////////////////////////////


UTexture* UCompositionUtilsDepthAlignmentPass::ApplyTransform_Implementation(UTexture* Input, UComposurePostProcessingPassProxy* PostProcessProxy, ACameraActor* TargetCamera)
{
	if (!Input)
		return Input;
	check(Input->GetResource());

	if (!(AuxiliaryCameraInputElement.IsValid() && VirtualCameraTargetActor.IsValid()))
	{
		UE_LOG(LogCompositionUtils, Warning, TEXT("DepthAlignmentPass: One of AuxiliaryCameraInputElement or VirtualCameraTargetActor has not been assigned."));
		return Input;
	}

	// Collect parameters
	// If any fails then this will pass through with a warning
	FDepthAlignmentParametersProxy ParametersProxy;

	if (VirtualCameraTargetActor.IsValid())
	{
		FMinimalViewInfo VirtualCameraView;
		VirtualCameraTargetActor->GetCameraComponent()->GetCameraView(0.0f, VirtualCameraView);

		FMatrix ProjectionMatrix = VirtualCameraView.CalculateProjectionMatrix();
		ParametersProxy.VirtualCam_ViewToNDC = static_cast<FMatrix44f>(ProjectionMatrix);
		ParametersProxy.VirtualCam_NDCToView = static_cast<FMatrix44f>(ProjectionMatrix.Inverse());
		ParametersProxy.VirtualCam_HorizontalFOV = FMath::DegreesToRadians(VirtualCameraView.FOV);
		ParametersProxy.VirtualCam_AspectRatio = VirtualCameraView.AspectRatio;
	}
	else
	{
		UE_LOG(LogCompositionUtils, Warning, TEXT("CompositionUtilsDepthAlignmentPass is not set up correctly: Virtual Camera Target Actor is missing!"));
		return Input;
	}

	// Update nodal offset transform
	{
		if (bResetCalibration)
		{
			bResetCalibration = false;
			CalibratedRotation = FQuat4f::Identity;
			CalibratedTranslation = FVector3f::ZeroVector;
		}

		FMatrix44f ExtrinsicMatrix = FMatrix44f::Identity;

		FQuat4f AlignTangentRotation{ FVector3f{ 0, 0, -1 }, FMath::DegreesToRadians(AlignmentRotationOffset) };
		FQuat4f Rotation = AlignTangentRotation * CalibratedRotation;

		FVector3f Translation = CalibratedTranslation + FVector3f(AlignmentTranslationOffset.X, AlignmentTranslationOffset.Y, 0);

		ExtrinsicMatrix = ExtrinsicMatrix.ConcatTranslation(Translation);
		ExtrinsicMatrix *= Rotation.ToMatrix();

		ParametersProxy.AuxiliaryToPrimaryNodalOffset = ExtrinsicMatrix;

		// This is so users can see what data is in the nodal offset matrix for debugging
		AuxiliaryToPrimaryNodalOffset.SetFromMatrix(static_cast<FMatrix>(ExtrinsicMatrix));
	}

	if (!CameraInput.IsValid())
		CameraInput = UCompositionUtilsCameraInput::TryGetCameraInputPassFromCompositingElement(AuxiliaryCameraInputElement);
	if (CameraInput.IsValid() && CameraInput->GetCameraIntrinsicData(ParametersProxy.AuxiliaryCameraData))
	{}
	else
	{
		UE_LOG(LogCompositionUtils, Warning, TEXT("CompositionUtilsDepthAlignmentPass is not set up correctly: Auxiliary Camera Input Element is missing!"));
		return Input;
	}

	ParametersProxy.HoleFillingBias = static_cast<uint32>(HoleFillingBias);

	// Calibration parameters
	{
		if (!InterestPointSpawnMin.ComponentwiseAllLessThan(InterestPointSpawnMax))
		{
			UE_LOG(LogCompositionUtils, Warning, TEXT("DepthAlignmentCalibration: Degenerate point spawning rulers! Max must be greater than min."))
			return Input;
		}
		ParametersProxy.CalibrationRulers = FVector4f{ InterestPointSpawnMin, InterestPointSpawnMax };
		ParametersProxy.CalibrationPointCount = static_cast<uint32>(CalibrationPointCount);
		ParametersProxy.bShowPoints = bShowCalibrationPoints;
	}

	FIntPoint Dims;
	Dims.X = Input->GetResource()->GetSizeX();
	Dims.Y = Input->GetResource()->GetSizeY();

	UTextureRenderTarget2D* RenderTarget = RequestRenderTarget(Dims, PF_FloatRGBA);
	if (!(RenderTarget && RenderTarget->GetResource()))
		return Input;

	ENQUEUE_RENDER_COMMAND(ApplyDepthAlignmentPass)(
		[this, bRunCalibration = bRunCalibrationOnNextPass,
				Parameters = ParametersProxy, InputResource = Input->GetResource(), OutputResource = RenderTarget->GetResource()]
		(FRHICommandListImmediate& RHICmdList)
		{
			FRDGBuilder GraphBuilder(RHICmdList);
			FRHIGPUBufferReadback CalibrationPointReadback("CalibrationPointReadback");

			TRefCountPtr<IPooledRenderTarget> InputRT = CreateRenderTarget(InputResource->GetTextureRHI(), TEXT("CompUtilsDepthAlignmentPass.Input"));
			TRefCountPtr<IPooledRenderTarget> OutputRT = CreateRenderTarget(OutputResource->GetTextureRHI(), TEXT("CompUtilsDepthAlignmentPass.Output"));

			// Set up RDG resources
			FRDGTextureRef InColorTexture = GraphBuilder.RegisterExternalTexture(InputRT);
			FRDGTextureRef OutColorTexture = GraphBuilder.RegisterExternalTexture(OutputRT);

			// Execute pipeline
			if (bRunCalibration)
			{
				CompositionUtils::ExecuteDepthAlignmentCalibrationPipeline(
					GraphBuilder,
					Parameters,
					InColorTexture,
					OutColorTexture,
					CalibrationPointReadback);
			}

			if (Parameters.bShowPoints)
			{
				CompositionUtils::VisualizeDepthAlignmentCalibrationPoints(
					GraphBuilder,
					Parameters,
					InColorTexture,
					OutColorTexture);
			}
			else
			{
				CompositionUtils::ExecuteDepthAlignmentPipeline(
					GraphBuilder,
					Parameters,
					InColorTexture,
					OutColorTexture);
			}

			GraphBuilder.Execute();

			if (bRunCalibration)
			{
				TFuture<void> Task = Async(EAsyncExecution::TaskGraph, [this, CalibrationPointReadbackTemp = std::move(CalibrationPointReadback)]
					{
						FRHIGPUBufferReadback& BufferReadback = *const_cast<FRHIGPUBufferReadback*>(&CalibrationPointReadbackTemp);

						while (!BufferReadback.IsReady()) {}

						ENQUEUE_RENDER_COMMAND(DepthAlignmentCalibrationReadback)(
							[this, BufferReadbackTemp = std::move(BufferReadback)]
							(FRHICommandListImmediate&)
							{
								FRHIGPUBufferReadback& BufferReadback = *const_cast<FRHIGPUBufferReadback*>(&BufferReadbackTemp);
								CalibrateAlignment_RenderThread(BufferReadback);
							});
					});
			}
		});

	// Reset calibration flag - don't want to execute calibration on successive frames
	bRunCalibrationOnNextPass = false;

	return RenderTarget;
}

#if WITH_EDITOR
void UCompositionUtilsDepthAlignmentPass::PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent)
{
	const FName PropertyName = PropertyChangedEvent.GetPropertyName();
	if (PropertyName == GET_MEMBER_NAME_CHECKED(UCompositionUtilsDepthAlignmentPass, AuxiliaryCameraInputElement))
	{
		CameraInput = nullptr;
	}

	Super::PostEditChangeProperty(PropertyChangedEvent);
}
#endif

void UCompositionUtilsDepthAlignmentPass::CalibrateAlignment_RenderThread(FRHIGPUBufferReadback& Readback)
{
	// Read back calibration points and perform calibration
	check(IsInRenderingThread());
	check(Readback.IsReady());

	TArray<FVector3f> Points;
	Points.SetNum(CalibrationPointCount);

	// Read bytes
	{
		uint64 NumBytes = sizeof(FVector3f) * CalibrationPointCount;
		uint8* Bytes = static_cast<uint8*>(Readback.Lock(NumBytes));
		FMemory::Memcpy(Points.GetData(), Bytes, NumBytes);
		Readback.Unlock();
	}

	// Calculate plane of best fit
	TOptional<FPlane4f> Plane = CompositionUtils::CalculatePlaneOfBestFit(Points);
	if (!Plane || !Plane->IsValid())
		return;

	// We are using the convention that the camera is looking along +Z
	// Therefore, a normal vector seen by the camera should be pointing somewhat towards it; so Z should always be less than 0
	if (Plane->GetNormal().Z > 0.0f)
		Plane = Plane->Flip();

	// Deduce transform
	FVector3f AlignmentTranslation;
	FQuat4f AlignmentRotation;
	{
		// This is done in three steps. First, the normals of the two planes are aligned.
		// Second, the tangents of the planes are aligned. This is based on a rotation tuned by the user.
		// Finally, translation is applied to line up the centroids of the planes. This is also additionally tuned by an offset supplied by user.

		FVector3f TargetNormal{ 0, 0, -1 };
		FVector3f TargetOrigin{ 0, 0, KnownDistance };

		FVector3f Normal = Plane->GetNormal();
		check(Normal.Normalize());
		FVector3f Centroid = Plane->GetOrigin();

		AlignmentTranslation = TargetOrigin - Centroid;

		// Construct basis for plane
		FVector3f Tangent = Normal ^ ((Normal == FVector3f{ 0, 1, 0 }) 
									? FVector3f{ 1, 0, 0 }
									: Normal ^ FVector3f{ 0, 1, 0 });
		FVector3f Bitangent = Normal ^ Tangent;
		check(Bitangent.Normalize());
		Tangent = Normal ^ Bitangent;
		check(Tangent.IsNormalized());

		// Step 1: Align normals
		float AngleBetweenNormals = FMath::Acos(Normal | TargetNormal);
		FVector3f RotationAxis = Normal ^ TargetNormal;
		check(RotationAxis.Normalize()); // Very important to normalize rotation axis

		AlignmentRotation = FQuat4f{ RotationAxis, AngleBetweenNormals };
	}

	// Send transform to game thread
	TFuture<void> Task = Async(EAsyncExecution::TaskGraphMainTick, [this, AlignmentTranslation, AlignmentRotation]
		{
			UpdateCalibration_GameThread(AlignmentTranslation, AlignmentRotation);
		});
}

void UCompositionUtilsDepthAlignmentPass::UpdateCalibration_GameThread(const FVector3f& Translation, const FQuat4f& Rotation)
{
	check(IsInGameThread());
	CalibratedTranslation = Translation;
	CalibratedRotation = Rotation;
}


//////////////////////////////////////
// UCompositionUtilsVolumetricsPass //
//////////////////////////////////////


UTexture* UCompositionUtilsVolumetricsPass::ApplyTransform_Implementation(UTexture* Input, UComposurePostProcessingPassProxy* PostProcessProxy, ACameraActor* TargetCamera)
{
	if (!Input || !CompUtilsCGLayer.IsValid())
		return Input;
	check(Input->GetResource());

	FIntPoint Dims;
	Dims.X = Input->GetResource()->GetSizeX();
	Dims.Y = Input->GetResource()->GetSizeY();

	UTextureRenderTarget2D* RenderTarget = RequestRenderTarget(Dims, PF_FloatRGBA);
	if (!(RenderTarget && RenderTarget->GetResource()))
		return Input;

	FVolumetricsCompositionParametersProxy Params;
	Params.VolumetricFogData = static_cast<const ACompositionUtilsCaptureBase*>(CompUtilsCGLayer.Get())->GetVolumetricFogData();
	if (!Params.VolumetricFogData || !Params.VolumetricFogData->IntegratedLightScatteringTexture)
		return Input;

	// Get the output of the depth pass
	bool bSuccess = PrePassLookupTable->FindNamedPassResult(CameraDepthPassName, Params.CameraDepthTexture);
	if (!bSuccess || !Params.CameraDepthTexture)
		return Input;

	ENQUEUE_RENDER_COMMAND(ApplyVolumetricCompositionPass)(
		[Parameters = MoveTemp(Params), InputResource = Input->GetResource(), OutputResource = RenderTarget->GetResource()]
		(FRHICommandListImmediate& RHICmdList)
		{
			FRDGBuilder GraphBuilder(RHICmdList);

			TRefCountPtr<IPooledRenderTarget> InputRT = CreateRenderTarget(InputResource->GetTextureRHI(), TEXT("CompUtilsVolumetricsPass.Input"));
			TRefCountPtr<IPooledRenderTarget> OutputRT = CreateRenderTarget(OutputResource->GetTextureRHI(), TEXT("CompUtilsVolumetricsPass.Output"));

			// Set up RDG resources
			FRDGTextureRef InColorTexture = GraphBuilder.RegisterExternalTexture(InputRT);
			FRDGTextureRef OutColorTexture = GraphBuilder.RegisterExternalTexture(OutputRT);

			// Execute pipeline
			CompositionUtils::ExecuteVolumetricsCompositionPipeline(
				GraphBuilder,
				Parameters,
				InColorTexture,
				OutColorTexture
			);

			GraphBuilder.Execute();
		});

	return RenderTarget;
}


/////////////////////////////////////
// UCompositionUtilsRelightingPass //
/////////////////////////////////////


UTexture* UCompositionUtilsRelightingPass::ApplyTransform_Implementation(UTexture* Input, UComposurePostProcessingPassProxy* PostProcessProxy, ACameraActor* TargetCamera)
{
	if (!Input)
		return Input;
	check(Input->GetResource());

	if (!TargetCamera || !TargetCamera->GetCameraComponent())
		return Input;

	FIntPoint Dims;
	Dims.X = Input->GetResource()->GetSizeX();
	Dims.Y = Input->GetResource()->GetSizeY();

	UTextureRenderTarget2D* RenderTarget = RequestRenderTarget(Dims, PF_FloatRGBA);
	if (!(RenderTarget && RenderTarget->GetResource()))
		return Input;

	FRelightingParametersProxy Params;
	bool bSuccess = PrePassLookupTable->FindNamedPassResult(CameraDepthPassName, Params.CameraDepthTexture);
	bSuccess &= PrePassLookupTable->FindNamedPassResult(CameraNormalPassName, Params.CameraNormalTexture);

	Params.LightProxy = (LightSource.IsValid() && LightSource->GetComponent()) ? LightSource->GetComponent()->SceneProxy : nullptr;

	// Get the camera view matrix
	FMinimalViewInfo CameraView;
	TargetCamera->GetCameraComponent()->GetCameraView(0.0f, CameraView);
	Params.CameraTransform.SetRotation(CameraView.Rotation.Quaternion());
	Params.CameraTransform.SetTranslation(CameraView.Location);

	Params.LightWeight = LightWeight;

	if (!bSuccess || !Params.IsValid())
		return Input;

	ENQUEUE_RENDER_COMMAND(ApplyRelightingPass)(
		[Parameters = MoveTemp(Params), InputResource = Input->GetResource(), OutputResource = RenderTarget->GetResource()]
		(FRHICommandListImmediate& RHICmdList)
		{
			FRDGBuilder GraphBuilder(RHICmdList);

			TRefCountPtr<IPooledRenderTarget> InputRT = CreateRenderTarget(InputResource->GetTextureRHI(), TEXT("CompUtilsRelightingPass.Input"));
			TRefCountPtr<IPooledRenderTarget> OutputRT = CreateRenderTarget(OutputResource->GetTextureRHI(), TEXT("CompUtilsRelightingPass.Output"));

			// Set up RDG resources
			FRDGTextureRef InColorTexture = GraphBuilder.RegisterExternalTexture(InputRT);
			FRDGTextureRef OutColorTexture = GraphBuilder.RegisterExternalTexture(OutputRT);

			// Execute pipeline
			CompositionUtils::ExecuteRelightingPipeline(
				GraphBuilder,
				Parameters,
				InColorTexture,
				OutColorTexture
			);

			GraphBuilder.Execute();
		});

	return RenderTarget;
}


///////////////////////////////////////
// UCompositionUtilsAddCrosshairPass //
///////////////////////////////////////


UTexture* UCompositionUtilsAddCrosshairPass::ApplyTransform_Implementation(UTexture* Input, UComposurePostProcessingPassProxy* PostProcessProxy, ACameraActor* TargetCamera)
{
	if (!Input)
		return Input;
	check(Input->GetResource());

	if (!TargetCamera || !TargetCamera->GetCameraComponent())
		return Input;

	FIntPoint Dims;
	Dims.X = Input->GetResource()->GetSizeX();
	Dims.Y = Input->GetResource()->GetSizeY();

	UTextureRenderTarget2D* RenderTarget = RequestRenderTarget(Dims, PF_FloatRGBA);
	if (!(RenderTarget && RenderTarget->GetResource()))
		return Input;

	ENQUEUE_RENDER_COMMAND(AddCrosshairPass)(
		[this, InputResource = Input->GetResource(), OutputResource = RenderTarget->GetResource()]
		(FRHICommandListImmediate& RHICmdList)
		{
			FRDGBuilder GraphBuilder(RHICmdList);

			TRefCountPtr<IPooledRenderTarget> InputRT = CreateRenderTarget(InputResource->GetTextureRHI(), TEXT("CompUtilsAddCrosshairPass.Input"));
			TRefCountPtr<IPooledRenderTarget> OutputRT = CreateRenderTarget(OutputResource->GetTextureRHI(), TEXT("CompUtilsAddCrosshairPass.Output"));
			FRDGTextureRef InTexture = GraphBuilder.RegisterExternalTexture(InputRT);
			FRDGTextureRef OutTexture = GraphBuilder.RegisterExternalTexture(OutputRT);

			CompositionUtils::ExecuteAddCrosshairPipeline(
				GraphBuilder,
				FVector4f(Color),
				static_cast<uint32>(Width),
				static_cast<uint32>(Length),
				InTexture,
				OutTexture
			);

			GraphBuilder.Execute();
		});

	return RenderTarget;
}


////////////////////////////////////////////
// UCompositionUtilsDepthPreviewPass //
////////////////////////////////////////////


UTexture* UCompositionUtilsDepthPreviewPass::ApplyTransform_Implementation(UTexture* Input, UComposurePostProcessingPassProxy* PostProcessProxy, ACameraActor* TargetCamera)
{
	if (!Input)
		return Input;
	check(Input->GetResource());

	if (!TargetCamera || !TargetCamera->GetCameraComponent())
		return Input;

	FIntPoint Dims;
	Dims.X = Input->GetResource()->GetSizeX();
	Dims.Y = Input->GetResource()->GetSizeY();

	UTextureRenderTarget2D* RenderTarget = RequestRenderTarget(Dims, PF_FloatRGBA);
	if (!(RenderTarget && RenderTarget->GetResource()))
		return Input;

	ApplyVisualizeDepth(Input, RenderTarget);

	return RenderTarget;
}

void UCompositionUtilsDepthPreviewPass::ApplyVisualizeDepth(UTexture* Input, UTextureRenderTarget2D* RenderTarget) const
{
	ENQUEUE_RENDER_COMMAND(ApplyDepthPreviewPass)(
		[DepthRange = VisualizeDepthRange, InputResource = Input->GetResource(), OutputResource = RenderTarget->GetResource()]
		(FRHICommandListImmediate& RHICmdList)
		{
			FRDGBuilder GraphBuilder(RHICmdList);

			TRefCountPtr<IPooledRenderTarget> InputRT = CreateRenderTarget(InputResource->GetTextureRHI(), TEXT("CompUtilsDepthPreviewPass.Input"));
			TRefCountPtr<IPooledRenderTarget> OutputRT = CreateRenderTarget(OutputResource->GetTextureRHI(), TEXT("CompUtilsDepthPreviewPass.Output"));
			FRDGTextureRef InTexture = GraphBuilder.RegisterExternalTexture(InputRT);
			FRDGTextureRef OutTexture = GraphBuilder.RegisterExternalTexture(OutputRT);

			CompositionUtils::VisualizeProcessedDepth(
				GraphBuilder,
				static_cast<FVector2f>(DepthRange),
				InTexture,
				OutTexture
			);

			GraphBuilder.Execute();
		});
}


////////////////////////////////////////////////
// UCompositionUtilsNormalMapPreviewPass //
////////////////////////////////////////////////

UTexture* UCompositionUtilsNormalMapPreviewPass::ApplyTransform_Implementation(UTexture* Input, UComposurePostProcessingPassProxy* PostProcessProxy, ACameraActor* TargetCamera)
{
	if (!Input)
		return Input;
	check(Input->GetResource());

	if (!TargetCamera || !TargetCamera->GetCameraComponent())
		return Input;

	FIntPoint Dims;
	Dims.X = Input->GetResource()->GetSizeX();
	Dims.Y = Input->GetResource()->GetSizeY();

	UTextureRenderTarget2D* RenderTarget = RequestRenderTarget(Dims, PF_FloatRGBA);
	if (!(RenderTarget && RenderTarget->GetResource()))
		return Input;

	FTransform LocalToWorldTransform;
	if (bDisplayWorldSpaceNormals)
	{
		FMinimalViewInfo CameraView;
		TargetCamera->GetCameraComponent()->GetCameraView(0.0f, CameraView);
		LocalToWorldTransform.SetRotation(CameraView.Rotation.Quaternion());
		LocalToWorldTransform.SetTranslation(CameraView.Location);
	}

	ENQUEUE_RENDER_COMMAND(ApplyNormalMapPreviewPass)(
		[bWorldSpace = bDisplayWorldSpaceNormals, LocalToWorld = LocalToWorldTransform, InputResource = Input->GetResource(), OutputResource = RenderTarget->GetResource()]
		(FRHICommandListImmediate& RHICmdList)
		{
			FRDGBuilder GraphBuilder(RHICmdList);

			TRefCountPtr<IPooledRenderTarget> InputRT = CreateRenderTarget(InputResource->GetTextureRHI(), TEXT("CompUtilsNormalMapPreviewPass.Input"));
			TRefCountPtr<IPooledRenderTarget> OutputRT = CreateRenderTarget(OutputResource->GetTextureRHI(), TEXT("CompUtilsNormalMapPreviewPass.Output"));
			FRDGTextureRef InTexture = GraphBuilder.RegisterExternalTexture(InputRT);
			FRDGTextureRef OutTexture = GraphBuilder.RegisterExternalTexture(OutputRT);

			CompositionUtils::VisualizeNormalMap(
				GraphBuilder,
				bWorldSpace,
				LocalToWorld,
				InTexture,
				OutTexture
			);

			GraphBuilder.Execute();
		});

	return RenderTarget;
}
