#include "Composure/CompUtilsElementTransforms.h"

#include "RenderGraphBuilder.h"
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

	if (!AuxiliaryCameraInput.IsValid())
	{
		if (AuxiliaryCameraInputElement.IsValid())
		{
			UTexture* Unused;
			if (UCompositingElementInput* InputPass = AuxiliaryCameraInputElement->FindInputPass(UCompositionUtilsAuxiliaryCameraInput::StaticClass(), Unused))
			{
				if (UCompositionUtilsAuxiliaryCameraInput* AuxiliaryCameraInputPass = Cast<UCompositionUtilsAuxiliaryCameraInput>(InputPass))
				{
					AuxiliaryCameraInput = AuxiliaryCameraInputPass;
				}
			}
		}
	}

	if (AuxiliaryCameraInput.IsValid())
	{
		Params.InvProjectionMatrix = AuxiliaryCameraInput->GetInverseProjectionMatrix();
		Params.CameraNearClippingPlane = AuxiliaryCameraInput->GetNearClippingPlane();
	}
	else
	{
		Params.InvProjectionMatrix = FMatrix44f::Identity;
		Params.CameraNearClippingPlane = 10.0f;
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



///////////////////////////////////////////
// UCompositionUtilsVolumetricsPass //
///////////////////////////////////////////


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


//////////////////////////////////////////
// UCompositionUtilsRelightingPass //
//////////////////////////////////////////


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
