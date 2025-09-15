#include "Composure/SlCompElementTransforms.h"

#include "RenderGraphBuilder.h"
#include "CompositingElements/ICompositingTextureLookupTable.h"

#include "SlCompEngineSubsystem.h"
#include "Composure/SlCompCaptureBase.h"
#include "Pipelines/SlCompPipelines.h"

#include "Components/DirectionalLightComponent.h"


///////////////////////////////////////////////
// UCompositingStereolabsDepthProcessingPass //
///////////////////////////////////////////////

UTexture* UCompositingStereolabsDepthProcessingPass::ApplyTransform_Implementation(UTexture* Input, UComposurePostProcessingPassProxy* PostProcessProxy, ACameraActor* TargetCamera)
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

	auto Subsystem = GEngine->GetEngineSubsystem<USlCompEngineSubsystem>();
	Params.InvProjectionMatrix = FMatrix44f(Subsystem->GetInvProjectionMatrix());
	Params.CameraNearClippingPlane = Subsystem->GetNearClippingPlane();

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

			TRefCountPtr<IPooledRenderTarget> InputRT = CreateRenderTarget(InputResource->GetTextureRHI(), TEXT("StereolabsDepthProcessingPass.Input"));
			TRefCountPtr<IPooledRenderTarget> OutputRT = CreateRenderTarget(OutputResource->GetTextureRHI(), TEXT("StereolabsDepthProcessingPass.Output"));

			// Set up RDG resources
			FRDGTextureRef InColorTexture = GraphBuilder.RegisterExternalTexture(InputRT);
			FRDGTextureRef OutColorTexture = GraphBuilder.RegisterExternalTexture(OutputRT);

			// Execute pipeline
			StereolabsCompositing::ExecuteDepthProcessingPipeline(
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
// UCompositingStereolabsVolumetricsPass //
///////////////////////////////////////////


UTexture* UCompositingStereolabsVolumetricsPass::ApplyTransform_Implementation(UTexture* Input, UComposurePostProcessingPassProxy* PostProcessProxy, ACameraActor* TargetCamera)
{
	if (!Input || !StereolabsCGLayer.IsValid())
		return Input;
	check(Input->GetResource());

	FIntPoint Dims;
	Dims.X = Input->GetResource()->GetSizeX();
	Dims.Y = Input->GetResource()->GetSizeY();

	UTextureRenderTarget2D* RenderTarget = RequestRenderTarget(Dims, PF_FloatRGBA);
	if (!(RenderTarget && RenderTarget->GetResource()))
		return Input;

	FVolumetricsCompositionParametersProxy Params;
	Params.VolumetricFogData = static_cast<const AStereolabsCompositingCaptureBase*>(StereolabsCGLayer.Get())->GetVolumetricFogData();
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

			TRefCountPtr<IPooledRenderTarget> InputRT = CreateRenderTarget(InputResource->GetTextureRHI(), TEXT("StereolabsVolumetricsPass.Input"));
			TRefCountPtr<IPooledRenderTarget> OutputRT = CreateRenderTarget(OutputResource->GetTextureRHI(), TEXT("StereolabsVolumetricsPass.Output"));

			// Set up RDG resources
			FRDGTextureRef InColorTexture = GraphBuilder.RegisterExternalTexture(InputRT);
			FRDGTextureRef OutColorTexture = GraphBuilder.RegisterExternalTexture(OutputRT);

			// Execute pipeline
			StereolabsCompositing::ExecuteVolumetricsCompositionPipeline(
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
// UCompositingStereolabsRelightingPass //
//////////////////////////////////////////


UTexture* UCompositingStereolabsRelightingPass::ApplyTransform_Implementation(UTexture* Input, UComposurePostProcessingPassProxy* PostProcessProxy, ACameraActor* TargetCamera)
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

			TRefCountPtr<IPooledRenderTarget> InputRT = CreateRenderTarget(InputResource->GetTextureRHI(), TEXT("StereolabsRelightingPass.Input"));
			TRefCountPtr<IPooledRenderTarget> OutputRT = CreateRenderTarget(OutputResource->GetTextureRHI(), TEXT("StereolabsRelightingPass.Output"));

			// Set up RDG resources
			FRDGTextureRef InColorTexture = GraphBuilder.RegisterExternalTexture(InputRT);
			FRDGTextureRef OutColorTexture = GraphBuilder.RegisterExternalTexture(OutputRT);

			// Execute pipeline
			StereolabsCompositing::ExecuteRelightingPipeline(
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
// UCompositingStereolabsDepthPreviewPass //
////////////////////////////////////////////


UTexture* UCompositingStereolabsDepthPreviewPass::ApplyTransform_Implementation(UTexture* Input, UComposurePostProcessingPassProxy* PostProcessProxy, ACameraActor* TargetCamera)
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

	if (bVisualizeReprojectionUVMap)
	{
		ApplyVisualizeReprojectionUVMap(Input, RenderTarget, TargetCamera);
	}
	else
	{
		ApplyVisualizeDepth(Input, RenderTarget);
	}

	return RenderTarget;
}

void UCompositingStereolabsDepthPreviewPass::ApplyVisualizeDepth(UTexture* Input, UTextureRenderTarget2D* RenderTarget) const
{
	ENQUEUE_RENDER_COMMAND(ApplyDepthPreviewPass)(
		[DepthRange = VisualizeDepthRange, InputResource = Input->GetResource(), OutputResource = RenderTarget->GetResource()]
		(FRHICommandListImmediate& RHICmdList)
		{
			FRDGBuilder GraphBuilder(RHICmdList);

			TRefCountPtr<IPooledRenderTarget> InputRT = CreateRenderTarget(InputResource->GetTextureRHI(), TEXT("StereolabsDepthPreviewPass.Input"));
			TRefCountPtr<IPooledRenderTarget> OutputRT = CreateRenderTarget(OutputResource->GetTextureRHI(), TEXT("StereolabsDepthPreviewPass.Output"));
			FRDGTextureRef InTexture = GraphBuilder.RegisterExternalTexture(InputRT);
			FRDGTextureRef OutTexture = GraphBuilder.RegisterExternalTexture(OutputRT);

			StereolabsCompositing::VisualizeProcessedDepth(
				GraphBuilder,
				static_cast<FVector2f>(DepthRange),
				InTexture,
				OutTexture
			);

			GraphBuilder.Execute();
		});
}

void UCompositingStereolabsDepthPreviewPass::ApplyVisualizeReprojectionUVMap(UTexture* Input, UTextureRenderTarget2D* RenderTarget, ACameraActor* TargetCamera) const
{
	ENQUEUE_RENDER_COMMAND(ApplyReprojectionUVMapPreviewPass)(
		[TargetCamera, InputResource = Input->GetResource(), OutputResource = RenderTarget->GetResource()]
		(FRHICommandListImmediate& RHICmdList)
		{
			FRDGBuilder GraphBuilder(RHICmdList);

			TRefCountPtr<IPooledRenderTarget> OutputRT = CreateRenderTarget(OutputResource->GetTextureRHI(), TEXT("StereolabsDepthPreviewPass.Output"));
			FRDGTextureRef OutTexture = GraphBuilder.RegisterExternalTexture(OutputRT);

			FMinimalViewInfo VirtualCameraView;
			TargetCamera->GetCameraComponent()->GetCameraView(0.0f, VirtualCameraView);

			// Execute pipeline
			FRDGTextureRef ReprojectionUVMap = StereolabsCompositing::CreateReprojectionUVMap(
				GraphBuilder,
				VirtualCameraView,
				FIntPoint{
					static_cast<int32>(InputResource->GetSizeX()),
					static_cast<int32>(InputResource->GetSizeY())
				}
			);

			StereolabsCompositing::VisualizeReprojectionUVMap(
				GraphBuilder,
				ReprojectionUVMap,
				OutTexture
			);

			GraphBuilder.Execute();
		});
}


////////////////////////////////////////////////
// UCompositionStereolabsNormalMapPreviewPass //
////////////////////////////////////////////////

UTexture* UCompositionStereolabsNormalMapPreviewPass::ApplyTransform_Implementation(UTexture* Input, UComposurePostProcessingPassProxy* PostProcessProxy, ACameraActor* TargetCamera)
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

			TRefCountPtr<IPooledRenderTarget> InputRT = CreateRenderTarget(InputResource->GetTextureRHI(), TEXT("StereolabsNormalMapPreviewPass.Input"));
			TRefCountPtr<IPooledRenderTarget> OutputRT = CreateRenderTarget(OutputResource->GetTextureRHI(), TEXT("StereolabsNormalMapPreviewPass.Output"));
			FRDGTextureRef InTexture = GraphBuilder.RegisterExternalTexture(InputRT);
			FRDGTextureRef OutTexture = GraphBuilder.RegisterExternalTexture(OutputRT);

			StereolabsCompositing::VisualizeNormalMap(
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
