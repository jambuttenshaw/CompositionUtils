#include "SlCompPipelines.h"

DECLARE_GPU_STAT_NAMED(SlCompVolumetricCompositionStat, TEXT("SlCompVolumetricComposition"));


class FVolumetricCompositionPS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FVolumetricCompositionPS)
	SHADER_USE_PARAMETER_STRUCT(FVolumetricCompositionPS, FGlobalShader)

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_STRUCT(FScreenPassTextureViewportParameters, OutViewPort)
		SHADER_PARAMETER_STRUCT(FScreenPassTextureViewportParameters, InViewPort)
		SHADER_PARAMETER_SAMPLER(SamplerState, sampler0)

		SHADER_PARAMETER_RDG_TEXTURE_SRV(Texture2D<float4>, CameraColorTexture)
		SHADER_PARAMETER_TEXTURE(Texture2D<float4>, CameraDepthTexture) // Not RDG resource

		SHADER_PARAMETER_RDG_TEXTURE_SRV(Texture3D, IntegratedLightScattering)
		SHADER_PARAMETER_SAMPLER(SamplerState, IntegratedLightScatteringSampler)

		SHADER_PARAMETER(float, VolumetricFogStartDistance)
		SHADER_PARAMETER(FVector3f, VolumetricFogInvGridSize)
		SHADER_PARAMETER(FVector3f, VolumetricFogGridZParams)
		SHADER_PARAMETER(FVector2f, VolumetricFogSVPosToVolumeUV)
		SHADER_PARAMETER(FVector2f, VolumetricFogUVMax)
		SHADER_PARAMETER(float, OneOverPreExposure)

		RENDER_TARGET_BINDING_SLOTS()
	END_SHADER_PARAMETER_STRUCT()
};

IMPLEMENT_GLOBAL_SHADER(FVolumetricCompositionPS, "/Plugin/StereolabsCompositing/VolumetricComposition.usf", "VolumetricCompositionPS", SF_Pixel);


void StereolabsCompositing::ExecuteVolumetricsCompositionPipeline(
	FRDGBuilder& GraphBuilder,
	const FVolumetricsCompositionParametersProxy& Parameters,
	FRDGTextureRef InTexture,
	FRDGTextureRef OutTexture
)
{
	check(IsInRenderingThread());
	check(Parameters.IsValid());

	RDG_EVENT_SCOPE_STAT(GraphBuilder, SlCompVolumetricCompositionStat, "SlCompVolumetricComposition");
	RDG_GPU_STAT_SCOPE(GraphBuilder, SlCompVolumetricCompositionStat);
	SCOPED_NAMED_EVENT(SlCompVolumetricCompositionStat, FColor::Purple);

	FRDGTextureRef IntegratedLightScatteringTexture = GraphBuilder.RegisterExternalTexture(Parameters.VolumetricFogData->IntegratedLightScatteringTexture);

	StereolabsCompositing::AddPass<FVolumetricCompositionPS, TStaticSamplerState<SF_Bilinear>>(
		GraphBuilder,
		RDG_EVENT_NAME("SlCompVolumetricComposition"),
		OutTexture,
		[&](auto PassParameters)
		{
			PassParameters->CameraColorTexture = GraphBuilder.CreateSRV(InTexture);
			PassParameters->CameraDepthTexture = Parameters.CameraDepthTexture->GetResource()->TextureRHI;

			PassParameters->IntegratedLightScattering = GraphBuilder.CreateSRV(IntegratedLightScatteringTexture);
			PassParameters->IntegratedLightScatteringSampler = TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();

			PassParameters->VolumetricFogStartDistance = Parameters.VolumetricFogData->VolumetricFogStartDistance;
			PassParameters->VolumetricFogInvGridSize = Parameters.VolumetricFogData->VolumetricFogInvGridSize;
			PassParameters->VolumetricFogGridZParams = Parameters.VolumetricFogData->VolumetricFogGridZParams;
			PassParameters->VolumetricFogSVPosToVolumeUV = Parameters.VolumetricFogData->VolumetricFogSVPosToVolumeUV;
			PassParameters->VolumetricFogUVMax = Parameters.VolumetricFogData->VolumetricFogUVMax;
			PassParameters->OneOverPreExposure = Parameters.VolumetricFogData->OneOverPreExposure;
		}
	);
}
