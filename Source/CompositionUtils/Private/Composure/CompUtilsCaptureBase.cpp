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
	return VolumetricFogData_RenderThread.Get();
}

FVolumetricFogRequiredDataProxy* ACompositionUtilsCaptureBase::GetVolumetricFogData()
{
	return VolumetricFogData_RenderThread.Get();
}

void ACompositionUtilsCaptureBase::FetchLatestCameraTextures_GameThread()
{
	check(!IsInRenderingThread());

	FCameraTexturesProxy Textures;
	Textures.ColorTexture = FindNamedRenderResult(CameraColorPassName);
	Textures.DepthTexture = FindNamedRenderResult(CameraDepthPassName);
	Textures.NormalsTexture = FindNamedRenderResult(CameraNormalsPassName);

	if (!AuxiliaryCameraInput.IsValid())
	{
		AuxiliaryCameraInput = UCompositionUtilsAuxiliaryCameraInput::TryGetAuxCameraInputPassFromCompositingElement(AuxiliaryCameraInputElement);
	}

	if (AuxiliaryCameraInput.IsValid())
	{
		(void)AuxiliaryCameraInput->GetCameraData(Textures.AuxiliaryCameraData);
	}

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
