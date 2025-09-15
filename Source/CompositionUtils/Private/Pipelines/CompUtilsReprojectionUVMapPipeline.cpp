#include "CompUtilsPipelines.h"

#include "Composure/CompUtilsCaptureBase.h"


class FReprojectionUVMapPS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FReprojectionUVMapPS);
	SHADER_USE_PARAMETER_STRUCT(FReprojectionUVMapPS, FGlobalShader)

	class FDisableReprojection : SHADER_PERMUTATION_BOOL("DISABLE_REPROJECTION");
	using FPermutationDomain = TShaderPermutationDomain<FDisableReprojection>;

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER(FMatrix44f, VirtualCameraViewToNDC)
		SHADER_PARAMETER(FMatrix44f, VirtualCameraNDCToView)

		SHADER_PARAMETER(FMatrix44f, DepthCameraViewToNDC)
		SHADER_PARAMETER(FMatrix44f, DepthCameraNDCToView)

		SHADER_PARAMETER(float, DepthCameraNearClippingPlane)

		SHADER_PARAMETER(FMatrix44f, InvDepthCameraNodalOffset)

		RENDER_TARGET_BINDING_SLOTS()
	END_SHADER_PARAMETER_STRUCT()
};

IMPLEMENT_GLOBAL_SHADER(FReprojectionUVMapPS, "/Plugin/CompositionUtils/ReprojectionUVMap.usf", "ReprojectionUVMapPS", SF_Pixel)

class FVisualizeReprojectionUVMapPS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FVisualizeReprojectionUVMapPS)
	SHADER_USE_PARAMETER_STRUCT(FVisualizeReprojectionUVMapPS, FGlobalShader)

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_STRUCT(FScreenPassTextureViewportParameters, OutViewPort)
		SHADER_PARAMETER_STRUCT(FScreenPassTextureViewportParameters, InViewPort)
		SHADER_PARAMETER_SAMPLER(SamplerState, sampler0)

		SHADER_PARAMETER_RDG_TEXTURE_SRV(Texture2D<float2>, ReprojectionUVMap)

		RENDER_TARGET_BINDING_SLOTS()
	END_SHADER_PARAMETER_STRUCT()
};

IMPLEMENT_GLOBAL_SHADER(FVisualizeReprojectionUVMapPS, "/Plugin/CompositionUtils/ReprojectionUVMap.usf", "VisualizeReprojectionUVMapPS", SF_Pixel);


FRDGTextureRef CompositionUtils::CreateReprojectionUVMap(
	FRDGBuilder& GraphBuilder, 
	const FMinimalViewInfo& VirtualCameraView, 
	FIntPoint TextureExtent,
	const FCameraTexturesProxy& CameraTextures,
	bool bPassThrough
)
{
	check(IsInRenderingThread());

	// Create output texture to hold UV map
	FRDGTextureRef  ReprojectionUVMap = GraphBuilder.CreateTexture(
		FRDGTextureDesc::Create2D(
			TextureExtent,
			PF_G16R16F, FClearValueBinding::None, ETextureCreateFlags::ShaderResource | ETextureCreateFlags::RenderTargetable),
		TEXT("CompUtils.ReprojectionUVMap")
	);

	{
		FReprojectionUVMapPS::FParameters* PassParameters = GraphBuilder.AllocParameters<FReprojectionUVMapPS::FParameters>();

		{
			FMatrix CameraProjectionMatrix = VirtualCameraView.CalculateProjectionMatrix();
			PassParameters->VirtualCameraViewToNDC = static_cast<FMatrix44f>(CameraProjectionMatrix);
			PassParameters->VirtualCameraNDCToView = static_cast<FMatrix44f>(CameraProjectionMatrix.Inverse());
		}

		{
			PassParameters->DepthCameraViewToNDC = CameraTextures.ViewToNDCMatrix;
			PassParameters->DepthCameraNDCToView = CameraTextures.NDCToViewMatrix;
			PassParameters->DepthCameraNearClippingPlane = CameraTextures.NearClipPlane;

			PassParameters->InvDepthCameraNodalOffset = FMatrix44f::Identity;
		}

		PassParameters->RenderTargets[0] = FRenderTargetBinding(ReprojectionUVMap, ERenderTargetLoadAction::ENoAction);

		FGlobalShaderMap* ShaderMap = GetGlobalShaderMap(GMaxRHIFeatureLevel);
		FReprojectionUVMapPS::FPermutationDomain Permutation;
		Permutation.Set<FReprojectionUVMapPS::FDisableReprojection>(bPassThrough);
		TShaderMapRef<FReprojectionUVMapPS> PixelShader(ShaderMap, Permutation);

		FScreenPassTextureViewport ViewPort(TextureExtent);

		AddDrawScreenPass(
			GraphBuilder,
			RDG_EVENT_NAME("CompUtilsReprojectionUVMap"),
			FScreenPassViewInfo{ GMaxRHIFeatureLevel },
			ViewPort,
			ViewPort,
			PixelShader,
			PassParameters
		);
	}

	return ReprojectionUVMap;
}


void CompositionUtils::VisualizeReprojectionUVMap(FRDGBuilder& GraphBuilder, FRDGTextureRef ReprojectionUVMap, FRDGTextureRef OutTexture)
{
	check(IsInRenderingThread());

	CompositionUtils::AddPass<FVisualizeReprojectionUVMapPS>(
		GraphBuilder,
		RDG_EVENT_NAME("CompUtilsVisualizeReprojectionUVMap"),
		OutTexture,
		[&](auto PassParameters)
		{
			PassParameters->ReprojectionUVMap = GraphBuilder.CreateSRV(ReprojectionUVMap);
		}
	);
}
