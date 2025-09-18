#include "CompUtilsPipelines.h"


class FAddCrosshairPS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FAddCrosshairPS)
	SHADER_USE_PARAMETER_STRUCT(FAddCrosshairPS, FGlobalShader)

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_STRUCT(FScreenPassTextureViewportParameters, OutViewPort)
		SHADER_PARAMETER_STRUCT(FScreenPassTextureViewportParameters, InViewPort)
		SHADER_PARAMETER_SAMPLER(SamplerState, sampler0)

		SHADER_PARAMETER_RDG_TEXTURE_SRV(Texture2D<float4>, InTex)

		SHADER_PARAMETER(FVector4f, CrosshairColor)
		SHADER_PARAMETER(uint32, CrosshairWidth)
		SHADER_PARAMETER(uint32, CrosshairLength)

		RENDER_TARGET_BINDING_SLOTS()
	END_SHADER_PARAMETER_STRUCT()
};

IMPLEMENT_GLOBAL_SHADER(FAddCrosshairPS, "/Plugin/CompositionUtils/AddCrosshair.usf", "AddCrosshairPS", SF_Pixel);


void CompositionUtils::ExecuteAddCrosshairPipeline(
	FRDGBuilder& GraphBuilder,
	const FVector4f& Color,
	uint32 Width,
	uint32 Length,
	FRDGTextureRef InTexture,
	FRDGTextureRef OutTexture
)
{
	check(IsInRenderingThread());

	CompositionUtils::AddPass<FAddCrosshairPS>(
		GraphBuilder,
		RDG_EVENT_NAME("CompUtils.AddCrossHair"),
		OutTexture,
		[&](auto PassParameters)
		{
			PassParameters->InTex = GraphBuilder.CreateSRV(InTexture);

			PassParameters->CrosshairColor = Color;
			PassParameters->CrosshairWidth = Width;
			PassParameters->CrosshairLength = Length;
		}
	);
}
