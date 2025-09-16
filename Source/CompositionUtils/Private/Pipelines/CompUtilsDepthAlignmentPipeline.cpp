#include "CompUtilsPipelines.h"

DECLARE_GPU_STAT_NAMED(CompUtilsDepthAlignmentStat, TEXT("CompUtilsDepthAlignment"));


class FCalculateUVMapPS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FCalculateUVMapPS)
	SHADER_USE_PARAMETER_STRUCT(FCalculateUVMapPS, FGlobalShader)

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_STRUCT(FScreenPassTextureViewportParameters, OutViewPort)
		SHADER_PARAMETER_STRUCT(FScreenPassTextureViewportParameters, InViewPort)
		SHADER_PARAMETER_SAMPLER(SamplerState, sampler0)

		SHADER_PARAMETER_RDG_TEXTURE_SRV(Texture2D<float4>, InTex)

		RENDER_TARGET_BINDING_SLOTS()
	END_SHADER_PARAMETER_STRUCT()
};

IMPLEMENT_GLOBAL_SHADER(FCalculateUVMapPS, "/Plugin/CompositionUtils/DepthAlignment.usf", "CalculateUVMapPS", SF_Pixel);


class FConvertDepthTextureToBufferCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FConvertDepthTextureToBufferCS)
	SHADER_USE_PARAMETER_STRUCT(FConvertDepthTextureToBufferCS, FGlobalShader)

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_RDG_TEXTURE_SRV(Texture2D<float4>, InDepthTexture)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<uint>, OutBuffer)

		SHADER_PARAMETER(FUintVector2, ViewDims)
	END_SHADER_PARAMETER_STRUCT()

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZE_2D"), GetThreadGroupSize2D());
	}

	static uint32 GetThreadGroupSize2D() { return 8; }
};

IMPLEMENT_GLOBAL_SHADER(FConvertDepthTextureToBufferCS, "/Plugin/CompositionUtils/DepthAlignment.usf", "ConvertDepthTextureToBufferCS", SF_Compute);


class FAlignDepthToColorCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FAlignDepthToColorCS)
	SHADER_USE_PARAMETER_STRUCT(FAlignDepthToColorCS, FGlobalShader)

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_RDG_BUFFER_SRV(Buffer<uint>, InBuffer)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<uint>, OutBuffer)

		SHADER_PARAMETER_RDG_TEXTURE_SRV(Texture2D<float2>, InUVMap)

		SHADER_PARAMETER(FUintVector2, ViewDims)
	END_SHADER_PARAMETER_STRUCT()

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZE_2D"), GetThreadGroupSize2D());
	}

	static uint32 GetThreadGroupSize2D() { return 8; }
};

IMPLEMENT_GLOBAL_SHADER(FAlignDepthToColorCS, "/Plugin/CompositionUtils/DepthAlignment.usf", "AlignDepthToColorCS", SF_Compute);


class FConvertBufferToDepthTextureCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FConvertBufferToDepthTextureCS)
	SHADER_USE_PARAMETER_STRUCT(FConvertBufferToDepthTextureCS, FGlobalShader)

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_RDG_BUFFER_SRV(Buffer<uint>, InBuffer)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<float4>, OutDepthTexture)

		SHADER_PARAMETER(FUintVector2, ViewDims)
	END_SHADER_PARAMETER_STRUCT()

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZE_2D"), GetThreadGroupSize2D());
	}

	static uint32 GetThreadGroupSize2D() { return 8; }
};

IMPLEMENT_GLOBAL_SHADER(FConvertBufferToDepthTextureCS, "/Plugin/CompositionUtils/DepthAlignment.usf", "ConvertBufferToDepthTextureCS", SF_Compute);


void CompositionUtils::ExecuteDepthAlignmentPipeline(
	FRDGBuilder& GraphBuilder,
	const FDepthAlignmentParametersProxy& Parameters,
	FRDGTextureRef InTexture, 
	FRDGTextureRef OutTexture)
{
	check(IsInRenderingThread());

	RDG_EVENT_SCOPE_STAT(GraphBuilder, CompUtilsDepthAlignmentStat, "CompUtilsDepthAlignment");
	RDG_GPU_STAT_SCOPE(GraphBuilder, CompUtilsDepthAlignmentStat);
	SCOPED_NAMED_EVENT(CompUtilsDepthAlignment, FColor::Purple);

	FIntPoint Extent = InTexture->Desc.Extent;

	FRDGTextureRef UVMap = GraphBuilder.CreateTexture(
		FRDGTextureDesc::Create2D(Extent, PF_G32R32F, FClearValueBinding::Black, TexCreate_RenderTargetable | TexCreate_ShaderResource),
		TEXT("CompUtils.DepthAlignment.UVMap")
	);

	// Create UV map
	CompositionUtils::AddPass<FCalculateUVMapPS>(
		GraphBuilder,
		RDG_EVENT_NAME("CompUtils.CalculateUVMap"),
		UVMap,
		[&](auto PassParameters)
		{
			PassParameters->InTex = GraphBuilder.CreateSRV(InTexture);
		}
	);

	FRDGTextureRef AlignedDepthTexture = GraphBuilder.CreateTexture(
		FRDGTextureDesc::Create2D(Extent, PF_FloatRGBA, FClearValueBinding::Black, TexCreate_ShaderResource | TexCreate_UAV),
		TEXT("CompUtils.DepthAlignment.AlignedDepth")
	);
	AddClearUAVPass(GraphBuilder, GraphBuilder.CreateUAV(AlignedDepthTexture), 0.0f);

	uint32 BufferWidth = Extent.X * Extent.Y;

	FRDGBufferDesc BufferDesc = FRDGBufferDesc::CreateBufferDesc(sizeof(uint32), BufferWidth);
	FRDGBufferRef BufferA = GraphBuilder.CreateBuffer(BufferDesc, TEXT("CompUtils.DepthAlignment.BufferA"));
	FRDGBufferRef BufferB = GraphBuilder.CreateBuffer(BufferDesc, TEXT("CompUtils.DepthAlignment.BufferB"));
	AddClearUAVPass(GraphBuilder, GraphBuilder.CreateUAV(BufferA, PF_R32_UINT), 0);
	AddClearUAVPass(GraphBuilder, GraphBuilder.CreateUAV(BufferB, PF_R32_UINT), 0);

	// Create aligned depth
	FIntVector GroupCount = FComputeShaderUtils::GetGroupCount(Extent, FAlignDepthToColorCS::GetThreadGroupSize2D());
	{
		FConvertDepthTextureToBufferCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FConvertDepthTextureToBufferCS::FParameters>();
		PassParameters->InDepthTexture = GraphBuilder.CreateSRV(InTexture);
		PassParameters->OutBuffer = GraphBuilder.CreateUAV(BufferA, PF_R32_UINT);
		PassParameters->ViewDims = FUintVector2(Extent.X, Extent.Y);

		FGlobalShaderMap* ShaderMap = GetGlobalShaderMap(GMaxRHIFeatureLevel);
		TShaderMapRef<FConvertDepthTextureToBufferCS> ComputeShader(ShaderMap);

		FComputeShaderUtils::AddPass(
			GraphBuilder,
			RDG_EVENT_NAME("CompUtils.ConvertDepthTextureToBuffer"),
			ERDGPassFlags::Compute,
			ComputeShader,
			PassParameters,
			GroupCount
		);
	}
	{
		FAlignDepthToColorCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FAlignDepthToColorCS::FParameters>();
		PassParameters->InBuffer = GraphBuilder.CreateSRV(BufferA, PF_R32_UINT);
		PassParameters->OutBuffer = GraphBuilder.CreateUAV(BufferB, PF_R32_UINT);
		PassParameters->InUVMap = GraphBuilder.CreateSRV(UVMap);
		PassParameters->ViewDims = FUintVector2( Extent.X, Extent.Y );

		FGlobalShaderMap* ShaderMap = GetGlobalShaderMap(GMaxRHIFeatureLevel);
		TShaderMapRef<FAlignDepthToColorCS> ComputeShader(ShaderMap);

		FComputeShaderUtils::AddPass(
			GraphBuilder,
			RDG_EVENT_NAME("CompUtils.AlignDepthToColor"),
			ERDGPassFlags::Compute,
			ComputeShader,
			PassParameters,
			GroupCount
		);
	}
	{
		FConvertBufferToDepthTextureCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FConvertBufferToDepthTextureCS::FParameters>();
		PassParameters->InBuffer = GraphBuilder.CreateSRV(BufferB, PF_R32_UINT);
		PassParameters->OutDepthTexture = GraphBuilder.CreateUAV(AlignedDepthTexture);
		PassParameters->ViewDims = FUintVector2(Extent.X, Extent.Y);

		FGlobalShaderMap* ShaderMap = GetGlobalShaderMap(GMaxRHIFeatureLevel);
		TShaderMapRef<FConvertBufferToDepthTextureCS> ComputeShader(ShaderMap);

		FComputeShaderUtils::AddPass(
			GraphBuilder,
			RDG_EVENT_NAME("CompUtils.ConvertBufferToDepthTexture"),
			ERDGPassFlags::Compute,
			ComputeShader,
			PassParameters,
			GroupCount
		);
	}

	// Copy into output
	AddCopyTexturePass(GraphBuilder, AlignedDepthTexture, OutTexture);
}
