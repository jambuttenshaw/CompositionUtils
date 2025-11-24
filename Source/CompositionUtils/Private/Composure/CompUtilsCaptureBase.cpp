#include "Composure/CompUtilsCaptureBase.h"

#include "CompUtilsViewExtension.h"
#include "Components/SceneCaptureComponent2D.h"

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
