#include "CompUtilsViewExtension.h"

#include "RenderGraphBuilder.h"
#include "SceneCore.h"
#include "ScenePrivate.h"
#include "SceneRendering.h"

#include "Composure/CompUtilsCaptureBase.h"
#include "Pipelines/CompUtilsPipelines.h"


// Some helper functions copied from the private renderer implementation
static FVector2f CompUtils_GetVolumetricFogUVMaxForSampling(const FVector2f& ViewRectSize, FIntVector VolumetricFogResourceGridSize, int32 VolumetricFogResourceGridPixelSize)
{
	float ViewRectSizeXSafe = FMath::DivideAndRoundUp<int32>(int32(ViewRectSize.X), VolumetricFogResourceGridPixelSize) * VolumetricFogResourceGridPixelSize - (VolumetricFogResourceGridPixelSize / 2 + 1);
	float ViewRectSizeYSafe = FMath::DivideAndRoundUp<int32>(int32(ViewRectSize.Y), VolumetricFogResourceGridPixelSize) * VolumetricFogResourceGridPixelSize - (VolumetricFogResourceGridPixelSize / 2 + 1);
	return FVector2f(ViewRectSizeXSafe, ViewRectSizeYSafe) / (FVector2f(VolumetricFogResourceGridSize.X, VolumetricFogResourceGridSize.Y) * VolumetricFogResourceGridPixelSize);
}

FVector CompUtils_GetVolumetricFogGridZParams(float VolumetricFogStartDistance, float NearPlane, float FarPlane, int32 GridSizeZ)
{
	// S = distribution scale
	// B, O are solved for given the z distances of the first+last slice, and the # of slices.
	//
	// slice = log2(z*B + O) * S

	// Don't spend lots of resolution right in front of the near plane

	NearPlane = FMath::Max(NearPlane, double(VolumetricFogStartDistance));

	double NearOffset = .095 * 100.0;
	// Space out the slices so they aren't all clustered at the near plane
	// TODO: This should be equal to the cvar declared in VolumetricFog.cpp, but it is not accessible from here
	constexpr float GVolumetricFogDepthDistributionScale = 32.0f;
	double S = GVolumetricFogDepthDistributionScale;

	double N = NearPlane + NearOffset;
	double F = FarPlane;

	double O = (F - N * FMath::Exp2((GridSizeZ - 1) / S)) / (F - N);
	double B = (1 - O) / N;

	return FVector(B, O, S);
}

// TODO: This should be equal to the cvar declared in VolumetricFog.cpp, but it is not accessible from here
constexpr int32 GVolumetricFogGridSizeZ = 64;
static int32 CompUtils_GetVolumetricFogGridSizeZ()
{
	return FMath::Max(1, GVolumetricFogGridSizeZ);
}

// TODO: This should be equal to the cvar declared in VolumetricFog.cpp, but it is not accessible from here
constexpr int32 GVolumetricFogGridPixelSize = 16;
int32 CompUtils_GetVolumetricFogGridPixelSize()
{
	return FMath::Max(1, GVolumetricFogGridPixelSize);
}

static FIntVector CompUtils_GetVolumetricFogGridSize(const FIntPoint& TargetResolution, int32& OutVolumetricFogGridPixelSize)
{
	FIntPoint VolumetricFogGridSizeXY;
	int32 VolumetricFogGridPixelSize = CompUtils_GetVolumetricFogGridPixelSize();
	VolumetricFogGridSizeXY = FIntPoint::DivideAndRoundUp(TargetResolution, VolumetricFogGridPixelSize);
	if (VolumetricFogGridSizeXY.X > GMaxVolumeTextureDimensions || VolumetricFogGridSizeXY.Y > GMaxVolumeTextureDimensions) //clamp to max volume texture dimensions. only happens for extreme resolutions (~8x2k)
	{
		float PixelSizeX = (float)TargetResolution.X / GMaxVolumeTextureDimensions;
		float PixelSizeY = (float)TargetResolution.Y / GMaxVolumeTextureDimensions;
		VolumetricFogGridPixelSize = FMath::Max(FMath::CeilToInt(PixelSizeX), FMath::CeilToInt(PixelSizeY));
		VolumetricFogGridSizeXY = FIntPoint::DivideAndRoundUp(TargetResolution, VolumetricFogGridPixelSize);
	}
	OutVolumetricFogGridPixelSize = VolumetricFogGridPixelSize;
	return FIntVector(VolumetricFogGridSizeXY.X, VolumetricFogGridSizeXY.Y, CompUtils_GetVolumetricFogGridSizeZ());
}

static FIntPoint CompUtils_GetVolumetricFogTextureResourceRes(const FViewInfo& View)
{
	// Allocate texture using scene render targets size so we do not reallocate every frame when dynamic resolution is used in order to avoid resources allocation hitches.
	FIntPoint BufferSize = View.GetSceneTexturesConfig().Extent;
	// Make sure the buffer size has some minimum resolution to make sure everything is always valid.
	BufferSize.X = FMath::Max(1, BufferSize.X);
	BufferSize.Y = FMath::Max(1, BufferSize.Y);
	return BufferSize;
}

FIntVector CompUtils_GetVolumetricFogResourceGridSize(const FViewInfo& View, int32& OutVolumetricFogGridPixelSize)
{
	return CompUtils_GetVolumetricFogGridSize(CompUtils_GetVolumetricFogTextureResourceRes(View), OutVolumetricFogGridPixelSize);
}


void FCompUtilsViewExtension::ExtractVolumetricFog(FRDGBuilder& GraphBuilder, FSceneView& View) const
{
	check(View.bIsSceneCapture && View.bIsViewInfo && CaptureActor.IsValid());

	FVolumetricFogRequiredDataProxy* VolumetricFogData = CaptureActor.Get()->GetVolumetricFogData();
	FViewInfo& ViewInfo = static_cast<FViewInfo&>(View);

	const FScene* Scene = static_cast<const FScene*>(View.Family->Scene);
	if (!Scene || Scene->ExponentialFogs.Num() == 0)
	{
		return;
	}
	const FExponentialHeightFogSceneInfo& FogInfo = Scene->ExponentialFogs[0];

	// Get fog info to pass along to composure
	if (auto Tex = ViewInfo.VolumetricFogResources.IntegratedLightScatteringTexture)
	{
		GraphBuilder.QueueTextureExtraction(Tex, &VolumetricFogData->IntegratedLightScatteringTexture);
	}

	// Get the properties required to be able to evaluate the volumetric fog in a composure pass
	int32 VolumetricFogGridPixelSize;
	const FIntVector VolumetricFogResourceGridSize = CompUtils_GetVolumetricFogResourceGridSize(ViewInfo, VolumetricFogGridPixelSize);

	VolumetricFogData->VolumetricFogStartDistance = ViewInfo.VolumetricFogStartDistance;
	VolumetricFogData->VolumetricFogInvGridSize = FVector3f::OneVector / static_cast<FVector3f>(VolumetricFogResourceGridSize);
	FVector ZParams = CompUtils_GetVolumetricFogGridZParams(ViewInfo.VolumetricFogStartDistance, ViewInfo.NearClippingDistance, FogInfo.VolumetricFogDistance, VolumetricFogResourceGridSize.Z);
	VolumetricFogData->VolumetricFogGridZParams = static_cast<FVector3f>(ZParams);
	VolumetricFogData->VolumetricFogSVPosToVolumeUV = FVector2f::UnitVector / (FVector2f(VolumetricFogResourceGridSize.X, VolumetricFogResourceGridSize.Y) * VolumetricFogGridPixelSize);
	VolumetricFogData->VolumetricFogUVMax = CompUtils_GetVolumetricFogUVMaxForSampling(ViewInfo.ViewRect.Size(), VolumetricFogResourceGridSize, VolumetricFogGridPixelSize);
	VolumetricFogData->OneOverPreExposure = 1.0f / ViewInfo.PreExposure;
}
