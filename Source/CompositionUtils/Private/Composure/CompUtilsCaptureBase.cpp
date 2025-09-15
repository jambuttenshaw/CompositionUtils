#include "Composure/SlCompCaptureBase.h"

#include "SceneViewExtension.h"
#include "SlCompViewExtension.h"
#include "Components/SceneCaptureComponent2D.h"

#include "Pipelines/SlCompPipelines.h"


AStereolabsCompositingCaptureBase::AStereolabsCompositingCaptureBase()
{
	PrimaryActorTick.bCanEverTick = true;

	if (!HasAnyFlags(RF_ClassDefaultObject))
	{
		SlCompViewExtension = MakeShared<FSlCompViewExtension>(this);
		SceneCaptureComponent2D->SceneViewExtensions.Add(SlCompViewExtension);

		VolumetricFogData_RenderThread = MakeShared<FVolumetricFogRequiredDataProxy>();
	}
}

const FVolumetricFogRequiredDataProxy* AStereolabsCompositingCaptureBase::GetVolumetricFogData() const
{
	return VolumetricFogData_RenderThread.Get();
}

FVolumetricFogRequiredDataProxy* AStereolabsCompositingCaptureBase::GetVolumetricFogData()
{
	return VolumetricFogData_RenderThread.Get();
}

void AStereolabsCompositingCaptureBase::FetchLatestCameraTextures_GameThread()
{
	check(!IsInRenderingThread());

	FStereolabsCameraTexturesProxy Textures;
	Textures.ColorTexture = FindNamedRenderResult(CameraColorPassName);
	Textures.DepthTexture = FindNamedRenderResult(CameraDepthPassName);
	Textures.NormalsTexture = FindNamedRenderResult(CameraNormalsPassName);

	ENQUEUE_RENDER_COMMAND(UpdateCameraTextures)(
	[this, TempTextures = MoveTemp(Textures)](FRHICommandListImmediate&)
	{
		CameraTextures_RenderThread = TempTextures;
	});
}

const FStereolabsCameraTexturesProxy& AStereolabsCompositingCaptureBase::GetCameraTextures_RenderThread() const
{
	check(IsInRenderingThread());
	return CameraTextures_RenderThread;
}
