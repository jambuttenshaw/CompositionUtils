#include "CompUtilsPipelines.h"

DECLARE_GPU_STAT_NAMED(CompUtilsDepthProcessingStat, TEXT("CompUtilsDepthProcessing"));


class FPreProcessDepthPS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FPreProcessDepthPS)
	SHADER_USE_PARAMETER_STRUCT(FPreProcessDepthPS, FGlobalShader)

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_STRUCT(FScreenPassTextureViewportParameters, OutViewPort)
		SHADER_PARAMETER_STRUCT(FScreenPassTextureViewportParameters, InViewPort)
		SHADER_PARAMETER_SAMPLER(SamplerState, sampler0)

		SHADER_PARAMETER_RDG_TEXTURE_SRV(Texture2D<float4>, InTex)

		RENDER_TARGET_BINDING_SLOTS()
	END_SHADER_PARAMETER_STRUCT()
};

IMPLEMENT_GLOBAL_SHADER(FPreProcessDepthPS, "/Plugin/CompositionUtils/DepthProcessing.usf", "PreProcessDepthPS", SF_Pixel);


class FRestrictPS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FRestrictPS)
	SHADER_USE_PARAMETER_STRUCT(FRestrictPS, FGlobalShader)

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_STRUCT(FScreenPassTextureViewportParameters, OutViewPort)
		SHADER_PARAMETER_STRUCT(FScreenPassTextureViewportParameters, InViewPort)
		SHADER_PARAMETER_SAMPLER(SamplerState, sampler0)

		SHADER_PARAMETER_RDG_TEXTURE_SRV(Texture2D<float4>, InTex)

		RENDER_TARGET_BINDING_SLOTS()
	END_SHADER_PARAMETER_STRUCT()
};

IMPLEMENT_GLOBAL_SHADER(FRestrictPS, "/Plugin/CompositionUtils/DepthProcessing.usf", "RestrictPS", SF_Pixel);


class FInterpolatePS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FInterpolatePS)
	SHADER_USE_PARAMETER_STRUCT(FInterpolatePS, FGlobalShader)

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_STRUCT(FScreenPassTextureViewportParameters, OutViewPort)
		SHADER_PARAMETER_STRUCT(FScreenPassTextureViewportParameters, InViewPort)
		SHADER_PARAMETER_SAMPLER(SamplerState, sampler0)

		SHADER_PARAMETER_RDG_TEXTURE_SRV(Texture2D<float4>, InTex)

		RENDER_TARGET_BINDING_SLOTS()
	END_SHADER_PARAMETER_STRUCT()
};

IMPLEMENT_GLOBAL_SHADER(FInterpolatePS, "/Plugin/CompositionUtils/DepthProcessing.usf", "InterpolatePS", SF_Pixel);


class FJacobiStepPS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FJacobiStepPS)
	SHADER_USE_PARAMETER_STRUCT(FJacobiStepPS, FGlobalShader)

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_STRUCT(FScreenPassTextureViewportParameters, OutViewPort)
		SHADER_PARAMETER_STRUCT(FScreenPassTextureViewportParameters, InViewPort)
		SHADER_PARAMETER_SAMPLER(SamplerState, sampler0)

		SHADER_PARAMETER_RDG_TEXTURE_SRV(Texture2D<float4>, InTex)

		RENDER_TARGET_BINDING_SLOTS()
	END_SHADER_PARAMETER_STRUCT()
};

IMPLEMENT_GLOBAL_SHADER(FJacobiStepPS, "/Plugin/CompositionUtils/DepthProcessing.usf", "JacobiStepPS", SF_Pixel);


// Post-processing on reconstructed depth, including clipping against specified planes
class FDepthClippingPS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FDepthClippingPS)
	SHADER_USE_PARAMETER_STRUCT(FDepthClippingPS, FGlobalShader)

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_STRUCT(FScreenPassTextureViewportParameters, OutViewPort)
		SHADER_PARAMETER_STRUCT(FScreenPassTextureViewportParameters, InViewPort)
		SHADER_PARAMETER_SAMPLER(SamplerState, sampler0)

		SHADER_PARAMETER(FMatrix44f, SourceNDCToView)
		SHADER_PARAMETER(int32, bEnableFarClipping)
		SHADER_PARAMETER(float, FarClipDistance)
		SHADER_PARAMETER(int32, bEnableClippingPlane)
		SHADER_PARAMETER(FVector4f, UserClippingPlane)

		SHADER_PARAMETER_RDG_TEXTURE_SRV(Texture2D<float4>, InTex)

		RENDER_TARGET_BINDING_SLOTS()
	END_SHADER_PARAMETER_STRUCT()
};

IMPLEMENT_GLOBAL_SHADER(FDepthClippingPS, "/Plugin/CompositionUtils/DepthProcessing.usf", "DepthClipPS", SF_Pixel);


class FVisualizeDepthPS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FVisualizeDepthPS)
	SHADER_USE_PARAMETER_STRUCT(FVisualizeDepthPS, FGlobalShader)

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_STRUCT(FScreenPassTextureViewportParameters, OutViewPort)
		SHADER_PARAMETER_STRUCT(FScreenPassTextureViewportParameters, InViewPort)
		SHADER_PARAMETER_SAMPLER(SamplerState, sampler0)

		SHADER_PARAMETER(FVector2f, DepthRange)

		SHADER_PARAMETER_RDG_TEXTURE_SRV(Texture2D<float4>, InTex)

		RENDER_TARGET_BINDING_SLOTS()
	END_SHADER_PARAMETER_STRUCT()
};

IMPLEMENT_GLOBAL_SHADER(FVisualizeDepthPS, "/Plugin/CompositionUtils/DepthProcessing.usf", "VisualizeDepthPS", SF_Pixel);


void CompositionUtils::ExecuteDepthProcessingPipeline(
	FRDGBuilder& GraphBuilder,
	const FDepthProcessingParametersProxy& Parameters,
	FRDGTextureRef InTexture,
	FRDGTextureRef OutTexture
)
{
	check(IsInRenderingThread());
	check(Parameters.SourceCamera.Type == ECompUtilsCameraType::CameraType_Physical && TEXT("Depth processing pipeline is designed to only work with physical cameras."));

	RDG_EVENT_SCOPE_STAT(GraphBuilder, CompUtilsDepthProcessingStat, "CompUtilsDepthProcessing");
	RDG_GPU_STAT_SCOPE(GraphBuilder, CompUtilsDepthProcessingStat);
	SCOPED_NAMED_EVENT(CompUtilsDepthProcessing, FColor::Purple);

	FRDGTextureRef TempTexture1 = CreateTextureFrom(GraphBuilder, OutTexture, TEXT("CompositionUtilsDepthProcessing.Temp1"));
	FRDGTextureRef TempTexture2 = CreateTextureFrom(GraphBuilder, OutTexture, TEXT("CompositionUtilsDepthProcessing.Temp2"));

	CompositionUtils::AddPass<FPreProcessDepthPS>(
		GraphBuilder,
		RDG_EVENT_NAME("PreProcessDepth"),
		TempTexture1,
		[&](auto PassParameters)
		{
			PassParameters->InTex = GraphBuilder.CreateSRV(InTexture);
		}
	);

	if (Parameters.bEnableJacobiSteps)
	{
		for (uint32 i = 0; i < Parameters.NumJacobiSteps; i++)
		{
			CompositionUtils::AddPass<FJacobiStepPS, TStaticSamplerState<>>(
				GraphBuilder,
				RDG_EVENT_NAME("JacobiStep(i=%d)", 2 * i),
				TempTexture2,
				[&](auto PassParameters)
				{
					PassParameters->InTex = GraphBuilder.CreateSRV(TempTexture1);
				}
			);

			CompositionUtils::AddPass<FJacobiStepPS, TStaticSamplerState<>>(
				GraphBuilder,
				RDG_EVENT_NAME("JacobiStep(i=%d)", 2 * i + 1),
				TempTexture1,
				[&](auto PassParameters)
				{
					PassParameters->InTex = GraphBuilder.CreateSRV(TempTexture2);
				}
			);
		}
	}

	// Post Processing
	CompositionUtils::AddPass<FDepthClippingPS, TStaticSamplerState<>>(
		GraphBuilder,
		RDG_EVENT_NAME("DepthClipping"),
		OutTexture,
		[&](auto PassParameters)
		{
			PassParameters->SourceNDCToView = Parameters.SourceCamera.NDCToView;

			PassParameters->bEnableFarClipping = Parameters.bEnableFarClipping;
			PassParameters->FarClipDistance = Parameters.FarClipDistance;
			PassParameters->bEnableClippingPlane = Parameters.bEnableClippingPlane;
			PassParameters->UserClippingPlane = Parameters.UserClippingPlane;

			PassParameters->InTex = GraphBuilder.CreateSRV(TempTexture1);
		}
	);
}


void CompositionUtils::VisualizeProcessedDepth(FRDGBuilder& GraphBuilder, FVector2f VisualizeRange, FRDGTextureRef ProcessedDepthTexture, FRDGTextureRef OutTexture)
{
	check(IsInRenderingThread());

	CompositionUtils::AddPass<FVisualizeDepthPS>(
		GraphBuilder,
		RDG_EVENT_NAME("VisualizeDepth"),
		OutTexture,
		[&](auto PassParameters)
		{
			PassParameters->DepthRange = VisualizeRange;

			PassParameters->InTex = GraphBuilder.CreateSRV(ProcessedDepthTexture);
		}
	);
}
