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
		Textures.ViewToNDCMatrix = AuxiliaryCameraInput->GetProjectionMatrix();
		Textures.NDCToViewMatrix = AuxiliaryCameraInput->GetInverseProjectionMatrix();
		Textures.NearClipPlane = AuxiliaryCameraInput->GetNearClippingPlane();
	}
	else
	{
		Textures.ViewToNDCMatrix = FMatrix44f::Identity;
		Textures.NDCToViewMatrix = FMatrix44f::Identity;
		Textures.NearClipPlane = 10.0f;
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
