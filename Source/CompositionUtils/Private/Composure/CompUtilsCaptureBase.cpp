#include "Composure/CompUtilsCaptureBase.h"

#include "CompUtilsViewExtension.h"
#include "Components/SceneCaptureComponent2D.h"
#include "CineCameraActor.h"

#include "Pipelines/CompUtilsPipelines.h"


ACompositionUtilsCaptureBase::ACompositionUtilsCaptureBase()
{
	PrimaryActorTick.bCanEverTick = true;

	if (!HasAnyFlags(RF_ClassDefaultObject))
	{
		CompUtilsViewExtension = MakeShared<FCompUtilsViewExtension>(this);
		SceneCaptureComponent2D->SceneViewExtensions.Add(CompUtilsViewExtension);

		VolumetricFogData_RenderThread = MakeShared<FVolumetricFogRequiredDataProxy>();
	}
}

const FVolumetricFogRequiredDataProxy* ACompositionUtilsCaptureBase::GetVolumetricFogData() const
{
	check(IsInRenderingThread());
	return VolumetricFogData_RenderThread.Get();
}

FVolumetricFogRequiredDataProxy* ACompositionUtilsCaptureBase::GetVolumetricFogData()
{
	check(IsInRenderingThread());
	return VolumetricFogData_RenderThread.Get();
}

void ACompositionUtilsCaptureBase::FetchLatestCameraTextures_GameThread()
{
	check(!IsInRenderingThread());

	FCameraTexturesProxy Textures;
	// FindNamedRenderResult must be called on game thread to avoid race conditions
	Textures.ColorTexture = FindNamedRenderResult(bUseOverrideColorPass ? OverrideCameraColorPassName : CameraColorPassName);
	Textures.DepthTexture = FindNamedRenderResult(CameraDepthPassName);
	Textures.NormalsTexture = FindNamedRenderResult(CameraNormalsPassName);

	ENQUEUE_RENDER_COMMAND(UpdateCameraTextures)(
	[this, TempTextures = MoveTemp(Textures)](FRHICommandListImmediate&)
	{
		CameraTextures_RenderThread = TempTextures;
	});
}

const FCameraTexturesProxy& ACompositionUtilsCaptureBase::GetCameraTextures_RenderThread() const
{
	check(IsInRenderingThread());
	return CameraTextures_RenderThread;
}

bool ACompositionUtilsCaptureBase::GetCameraIntrinsicData(FCompUtilsCameraIntrinsicData& OutData)
{
	ACameraActor* CameraActor = FindTargetCamera();
	if (!CameraActor)
	{
		return false;
	}

	FMinimalViewInfo VirtualCameraView;
	UCameraComponent* CameraComponent = CameraActor->GetCameraComponent();
	CameraComponent->GetCameraView(0.0f, VirtualCameraView);

	FMatrix ProjectionMatrix = VirtualCameraView.CalculateProjectionMatrix();

	OutData.Type = ECompUtilsCameraType::CameraType_Virtual;

	OutData.ViewToNDC = static_cast<FMatrix44f>(ProjectionMatrix);
	OutData.NDCToView = static_cast<FMatrix44f>(ProjectionMatrix.Inverse());

	OutData.HorizontalFOV = FMath::DegreesToRadians(VirtualCameraView.FOV);
	// Calculate vertical FOV from horizontal FOV
	OutData.VerticalFOV =
		2.0f * FMath::Atan(FMath::Tan(0.5f * OutData.HorizontalFOV) / VirtualCameraView.AspectRatio);

	if (UCineCameraComponent* CineCameraComponent = Cast<UCineCameraComponent>(CameraComponent))
	{
		// TODO: Populate camera intrinsics
	}
	else
	{
		// Intrinsics will be unavailable
	}

	return true;
}
