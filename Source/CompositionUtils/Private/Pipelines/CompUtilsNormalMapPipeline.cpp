#include "SlCompPipelines.h"


class FVisualizeNormalMapPS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FVisualizeNormalMapPS)
	SHADER_USE_PARAMETER_STRUCT(FVisualizeNormalMapPS, FGlobalShader)

	class FTransformToWorldSpace : SHADER_PERMUTATION_BOOL("VIEW_WORLD_SPACE");
	using FPermutationDomain = TShaderPermutationDomain<FTransformToWorldSpace>;

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_STRUCT(FScreenPassTextureViewportParameters, OutViewPort)
		SHADER_PARAMETER_STRUCT(FScreenPassTextureViewportParameters, InViewPort)
		SHADER_PARAMETER_SAMPLER(SamplerState, sampler0)

		SHADER_PARAMETER_RDG_TEXTURE_SRV(Texture2D<float4>, InTex)

		SHADER_PARAMETER(FMatrix44f, ViewLocalToWorld)

		RENDER_TARGET_BINDING_SLOTS()
	END_SHADER_PARAMETER_STRUCT()
};

IMPLEMENT_GLOBAL_SHADER(FVisualizeNormalMapPS, "/Plugin/StereolabsCompositing/NormalMapProcessing.usf", "VisualizeNormalMapPS", SF_Pixel);


void StereolabsCompositing::VisualizeNormalMap(
	FRDGBuilder& GraphBuilder,
	bool bWorldSpace,
	const FTransform& LocalToWorldTransform,
	FRDGTextureRef NormalMap,
	FRDGTextureRef OutTexture
)
{
	check(IsInRenderingThread());

	FVisualizeNormalMapPS::FPermutationDomain Permutation;
	Permutation.Set<FVisualizeNormalMapPS::FTransformToWorldSpace>(bWorldSpace);

	StereolabsCompositing::AddPass<FVisualizeNormalMapPS>(
		GraphBuilder,
		RDG_EVENT_NAME("VisualizeDepth"),
		OutTexture,
		[&](auto PassParameters)
		{
			PassParameters->ViewLocalToWorld = static_cast<FMatrix44f>(LocalToWorldTransform.ToMatrixNoScale());

			PassParameters->InTex = GraphBuilder.CreateSRV(NormalMap);
		},
		Permutation
	);
}
