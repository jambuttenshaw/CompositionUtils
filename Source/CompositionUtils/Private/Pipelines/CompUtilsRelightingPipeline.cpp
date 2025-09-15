#include "SlCompPipelines.h"

#include "ScreenPass.h"
#include "LightSceneProxy.h"

DECLARE_GPU_STAT_NAMED(SlCompRelightingStat, TEXT("SlCompRelighting"));


class FRelightingPS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FRelightingPS)
	SHADER_USE_PARAMETER_STRUCT(FRelightingPS, FGlobalShader)

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_STRUCT(FScreenPassTextureViewportParameters, OutViewPort)
		SHADER_PARAMETER_STRUCT(FScreenPassTextureViewportParameters, InViewPort)
		SHADER_PARAMETER_SAMPLER(SamplerState, sampler0)

		SHADER_PARAMETER_RDG_TEXTURE_SRV(Texture2D<float4>, CameraColorTexture)
		SHADER_PARAMETER_TEXTURE(Texture2D<float4>, CameraDepthTexture) // Not RDG resource
		SHADER_PARAMETER_TEXTURE(Texture2D<float4>, CameraNormalTexture) // Not RDG resource

		SHADER_PARAMETER(FVector3f, LightDirection)
		SHADER_PARAMETER(FVector3f, LightColor)

		SHADER_PARAMETER(FMatrix44f, CameraLocalToWorld)
		SHADER_PARAMETER(FMatrix44f, CameraWorldToLocal)

		SHADER_PARAMETER(float, LightWeight)

		RENDER_TARGET_BINDING_SLOTS()
	END_SHADER_PARAMETER_STRUCT()
};

IMPLEMENT_GLOBAL_SHADER(FRelightingPS, "/Plugin/StereolabsCompositing/Relighting.usf", "RelightingPS", SF_Pixel);


void StereolabsCompositing::ExecuteRelightingPipeline(
	FRDGBuilder& GraphBuilder,
	const FRelightingParametersProxy& Parameters,
	FRDGTextureRef InTexture,
	FRDGTextureRef OutTexture
)
{
	check(IsInRenderingThread());
	check(Parameters.IsValid());

	RDG_EVENT_SCOPE_STAT(GraphBuilder, SlCompRelightingStat, "SlCompRelighting");
	RDG_GPU_STAT_SCOPE(GraphBuilder, SlCompRelightingStat);
	SCOPED_NAMED_EVENT(SlCompRelightingStat, FColor::Purple);

	StereolabsCompositing::AddPass<FRelightingPS, TStaticSamplerState<>>(
		GraphBuilder,
		RDG_EVENT_NAME("SlCompRelighting"),
		OutTexture,
		[&](auto PassParameters)
		{
			PassParameters->CameraColorTexture = GraphBuilder.CreateSRV(InTexture);
			PassParameters->CameraDepthTexture = Parameters.CameraDepthTexture->GetResource()->TextureRHI;
			PassParameters->CameraNormalTexture = Parameters.CameraNormalTexture->GetResource()->TextureRHI;

			PassParameters->LightColor = static_cast<FVector3f>(Parameters.LightProxy->GetColor());
			PassParameters->LightDirection = static_cast<FVector3f>(Parameters.LightProxy->GetDirection());

			PassParameters->CameraLocalToWorld = static_cast<FMatrix44f>(Parameters.CameraTransform.ToMatrixNoScale());
			PassParameters->CameraWorldToLocal = PassParameters->CameraLocalToWorld.Inverse();

			PassParameters->LightWeight = Parameters.LightWeight;
		}
	);
}
