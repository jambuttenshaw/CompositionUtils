#include "Composure/CompUtilsElementTransforms.h"

#include "RenderGraphBuilder.h"
#include "RHIGPUReadback.h"
#include "TextureResource.h"

#include "Components/DirectionalLightComponent.h"

#include "CompositingElements/ICompositingTextureLookupTable.h"
#include "Composure/CompUtilsCaptureBase.h"

#include "CompositionUtils.h"
#include "CompUtilsCameraInterface.h"

#include "Pipelines/CompUtilsPipelines.h"


/////////////
// Helpers //
/////////////

// Searches the compositing element to see if itself or any of its input passes implement the CompUtilsCameraInterface
// Returns nullptr if no interface could be found
ICompUtilsCameraInterface* FindCameraInterfaceFromInputElement(ACompositingElement* CompositingElement)
{
	if (CompositingElement->Implements<UCompUtilsCameraInterface>())
	{
		return Cast<ICompUtilsCameraInterface>(CompositingElement);
	}

	// Search inputs
	for (auto Input : CompositingElement->GetInputsList())
	{
		if (Input->Implements<UCompUtilsCameraInterface>())
		{
			return Cast<ICompUtilsCameraInterface>(Input);
		}
	}

	return nullptr;
}

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

	if (SourceCamera.IsValid())
	{
		if (auto Interface = FindCameraInterfaceFromInputElement(SourceCamera.Get()))
		{
			Interface->GetCameraIntrinsicData(Params.SourceCamera);
		}
	}
	else
	{
		UE_LOG(LogCompositionUtils, Warning, TEXT("DepthProcessingPass: SourceCamera is missing or doesn't implement CompUtils CameraInterface!"));
		return Input;
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


/////////////////////////////////////////
// UCompositionUtilsDepthAlignmentPass //
/////////////////////////////////////////


UTexture* UCompositionUtilsDepthAlignmentPass::ApplyTransform_Implementation(UTexture* Input, UComposurePostProcessingPassProxy* PostProcessProxy, ACameraActor*)
{
	if (!Input)
		return Input;
	check(Input->GetResource());

	if (!(SourceCamera.IsValid() && TargetCamera.IsValid()))
	{
		UE_LOG(LogCompositionUtils, Warning, TEXT("DepthAlignmentPass: One of SourceCamera or TargetCamera has not been assigned."));
		return Input;
	}

	// Collect parameters
	// If any fails then this will pass through with a warning
	FDepthAlignmentParametersProxy ParametersProxy;

	if (SourceCamera.IsValid())
	{
		if (auto Interface = FindCameraInterfaceFromInputElement(SourceCamera.Get()))
		{
			Interface->GetCameraIntrinsicData(ParametersProxy.SourceCamera);
		}
	}
	else
	{
		UE_LOG(LogCompositionUtils, Warning, TEXT("DepthAlignmentPass: SourceCamera is missing or doesn't implement CompUtils CameraInterface!"));
		return Input;
	}

	if (TargetCamera.IsValid())
	{
		if (auto Interface = FindCameraInterfaceFromInputElement(TargetCamera.Get()))
		{
			Interface->GetCameraIntrinsicData(ParametersProxy.TargetCamera);
		}
	}
	else
	{
		UE_LOG(LogCompositionUtils, Warning, TEXT("DepthAlignmentPass: TargetCamera is missing or doesn't implement CompUtils CameraInterface!"));
		return Input;
	}

	// Update nodal offset transform
	{
		FMatrix44f ExtrinsicMatrix = FMatrix44f::Identity;

		FQuat4f AlignTangentRotation{ FVector3f{ 0, 0, -1 }, FMath::DegreesToRadians(AlignmentRotationOffset) };
		FQuat4f Rotation = AlignTangentRotation;
		FVector3f Translation = FVector3f(AlignmentTranslationOffset.X, AlignmentTranslationOffset.Y, 0);

		ExtrinsicMatrix = ExtrinsicMatrix.ConcatTranslation(Translation);
		ExtrinsicMatrix *= Rotation.ToMatrix();

		ParametersProxy.SourceToTargetNodalOffset = ExtrinsicMatrix;

		// This is so users can see what data is in the nodal offset matrix for debugging
		SourceToTargetNodalOffset.SetFromMatrix(static_cast<FMatrix>(ExtrinsicMatrix));
	}

	ParametersProxy.HoleFillingBias = static_cast<uint32>(HoleFillingBias);

	FIntPoint Dims;
	Dims.X = Input->GetResource()->GetSizeX();
	Dims.Y = Input->GetResource()->GetSizeY();

	UTextureRenderTarget2D* RenderTarget = RequestRenderTarget(Dims, PF_FloatRGBA);
	if (!(RenderTarget && RenderTarget->GetResource()))
		return Input;

	ENQUEUE_RENDER_COMMAND(ApplyDepthAlignmentPass)(
		[this, Parameters = ParametersProxy, InputResource = Input->GetResource(), OutputResource = RenderTarget->GetResource()]
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
			CompositionUtils::ExecuteDepthAlignmentPipeline(
				GraphBuilder,
				Parameters,
				InColorTexture,
				OutColorTexture);
			
			GraphBuilder.Execute();

			/*
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
			*/
		});

	return RenderTarget;
}

/*
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
*/

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


///////////////////////////////////////////
// UCompositionUtilsNormalMapPreviewPass //
///////////////////////////////////////////

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


/////////////////////////////////////////
// UCompositionUtilsTextureMappingPass //
/////////////////////////////////////////

UTexture* UCompositionUtilsTextureMappingPass::ApplyTransform_Implementation(UTexture* Input, UComposurePostProcessingPassProxy* PostProcessProxy, ACameraActor* TargetCamera)
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

	UTexture* AlignedDepth = nullptr;
	bool bSuccess = PrePassLookupTable->FindNamedPassResult(AlignedDepthPassName, AlignedDepth);

	if (!bSuccess || !AlignedDepth)
		return Input;

	ENQUEUE_RENDER_COMMAND(ApplyRelightingPass)(
		[TextureToMapResource = Input->GetResource(), AlignedDepthResource = AlignedDepth->GetResource(), OutputResource = RenderTarget->GetResource()]
		(FRHICommandListImmediate& RHICmdList)
		{
			FRDGBuilder GraphBuilder(RHICmdList);

			TRefCountPtr<IPooledRenderTarget> TextureToMapRT = CreateRenderTarget(TextureToMapResource->GetTextureRHI(), TEXT("CompUtilsTextureMappingPass.TextureToMap"));
			TRefCountPtr<IPooledRenderTarget> AlignedDepthRT = CreateRenderTarget(AlignedDepthResource->GetTextureRHI(), TEXT("CompUtilsTextureMappingPass.AlignedDepth"));
			TRefCountPtr<IPooledRenderTarget> OutputRT = CreateRenderTarget(OutputResource->GetTextureRHI(), TEXT("CompUtilsTextureMappingPass.Output"));

			// Set up RDG resources
			FRDGTextureRef InTextureToMap = GraphBuilder.RegisterExternalTexture(TextureToMapRT);
			FRDGTextureRef InAlignedDepth = GraphBuilder.RegisterExternalTexture(AlignedDepthRT);
			FRDGTextureRef OutTexture = GraphBuilder.RegisterExternalTexture(OutputRT);

			// Execute pipeline
			CompositionUtils::ExecuteTextureMappingPipeline(
				GraphBuilder,
				InTextureToMap,
				InAlignedDepth,
				OutTexture
			);

			GraphBuilder.Execute();
		});

	return RenderTarget;
}


///////////////////////////////////////
// UCompositionUtilsImageComparePass //
///////////////////////////////////////

UTexture* UCompositionUtilsCompareTexturesPass::ApplyTransform_Implementation(UTexture* Input, UComposurePostProcessingPassProxy* PostProcessProxy, ACameraActor* TargetCamera)
{
	UTexture* TextureA = nullptr;
	UTexture* TextureB = nullptr;
	bool bSuccess = PrePassLookupTable->FindNamedPassResult(TextureAPassName, TextureA);
	bSuccess	 |= PrePassLookupTable->FindNamedPassResult(TextureBPassName, TextureB);

	if (!bSuccess)
		return Input;

	return bShowA ? TextureA : TextureB;
}
