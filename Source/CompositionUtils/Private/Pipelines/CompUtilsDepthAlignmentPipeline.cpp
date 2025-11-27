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

		SHADER_PARAMETER(FMatrix44f, SourceNDCToView)
		SHADER_PARAMETER(FMatrix44f, SourceToDestinationNodalOffset)
		SHADER_PARAMETER(FMatrix44f, DestinationViewToNDC)

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
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<uint64_t>, OutBuffer)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<uint64_t>, InitialClearBuffer)

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
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<uint64_t>, InBuffer)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<uint64_t>, OutBuffer)

		SHADER_PARAMETER_RDG_TEXTURE_SRV(Texture2D<float2>, InUVMap)

		SHADER_PARAMETER(FUintVector2, ViewDims)
		SHADER_PARAMETER(FUintVector2, PatchSize)
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
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<uint64_t>, InBuffer)
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
	check(Parameters.SourceCamera.Type == ECompUtilsCameraType::CameraType_Physical && 
		TEXT("Depth alignment pipeline currently only supports using physical cameras as a source. This is due to how deprojection is implemented."));

	RDG_EVENT_SCOPE_STAT(GraphBuilder, CompUtilsDepthAlignmentStat, "CompUtilsDepthAlignment");
	RDG_GPU_STAT_SCOPE(GraphBuilder, CompUtilsDepthAlignmentStat);
	SCOPED_NAMED_EVENT(CompUtilsDepthAlignment, FColor::Purple);

	FIntPoint Extent = InTexture->Desc.Extent;

	FRDGTextureDesc UVMapDesc = FRDGTextureDesc::Create2D(Extent, PF_G32R32F, FClearValueBinding{ {-1, -1, -1, -1} }, TexCreate_RenderTargetable | TexCreate_ShaderResource | TexCreate_UAV);
	FRDGTextureRef UVMap = GraphBuilder.CreateTexture(UVMapDesc, TEXT("CompUtils.DepthAlignment.UVMap"));

	FRDGTextureRef AlignedDepthTexture = GraphBuilder.CreateTexture(
		FRDGTextureDesc::Create2D(Extent, PF_FloatRGBA, FClearValueBinding::Black, TexCreate_ShaderResource | TexCreate_UAV),
		TEXT("CompUtils.DepthAlignment.AlignedDepth")
	);
	AddClearUAVPass(GraphBuilder, GraphBuilder.CreateUAV(AlignedDepthTexture), 0.0f);

	uint32 BufferWidth = Extent.X * Extent.Y;
	FRDGBufferRef BufferA = CreateStructuredBuffer(GraphBuilder, TEXT("CompUtils.DepthAlignment.BufferA"), sizeof(uint64), BufferWidth, nullptr, 0);
	FRDGBufferRef BufferB = CreateStructuredBuffer(GraphBuilder, TEXT("CompUtils.DepthAlignment.BufferB"), sizeof(uint64), BufferWidth, nullptr, 0);

	// Create UV map
	CompositionUtils::AddPass<FCalculateUVMapPS>(
		GraphBuilder,
		RDG_EVENT_NAME("CompUtils.CalculateUVMap"),
		UVMap,
		[&](auto PassParameters)
		{
			PassParameters->InTex = GraphBuilder.CreateSRV(InTexture);

			PassParameters->SourceNDCToView = Parameters.SourceCamera.NDCToView;
			PassParameters->SourceToDestinationNodalOffset = Parameters.SourceToDestinationNodalOffset;
			PassParameters->DestinationViewToNDC = Parameters.DestinationCamera.ViewToNDC;
		}
	);

	// Create aligned depth
	FIntVector GroupCount = FComputeShaderUtils::GetGroupCount(Extent, FAlignDepthToColorCS::GetThreadGroupSize2D());
	{
		FConvertDepthTextureToBufferCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FConvertDepthTextureToBufferCS::FParameters>();
		PassParameters->InDepthTexture = GraphBuilder.CreateSRV(InTexture);
		PassParameters->OutBuffer = GraphBuilder.CreateUAV(BufferA);
		PassParameters->InitialClearBuffer = GraphBuilder.CreateUAV(BufferB);
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
		PassParameters->InBuffer = GraphBuilder.CreateSRV(BufferA);
		PassParameters->OutBuffer = GraphBuilder.CreateUAV(BufferB);
		PassParameters->InUVMap = GraphBuilder.CreateSRV(UVMap);
		PassParameters->ViewDims = FUintVector2( Extent.X, Extent.Y );

		// Calculate how big a patch size is required to avoid holes
		{
			float Physical_TanHalfFOVX = FMath::Tan(0.5f * Parameters.SourceCamera.HorizontalFOV);
			float Physical_TanHalfFOVY = FMath::Tan(0.5f * Parameters.SourceCamera.VerticalFOV);
			float Virtual_TanHalfFOVX  = FMath::Tan(0.5f * Parameters.DestinationCamera.HorizontalFOV);
			float Virtual_TanHalfFOVY  = FMath::Tan(0.5f * Parameters.DestinationCamera.VerticalFOV);
			FIntPoint PatchSize = {
				FMath::CeilToInt(Physical_TanHalfFOVX / Virtual_TanHalfFOVX),
				FMath::CeilToInt(Physical_TanHalfFOVY / Virtual_TanHalfFOVY)
			};

			PassParameters->PatchSize = FUintVector2{
				static_cast<uint32>(FMath::Clamp(PatchSize.X + Parameters.HoleFillingBias, 1, 16)),
				static_cast<uint32>(FMath::Clamp(PatchSize.Y + Parameters.HoleFillingBias, 1, 16))
			};
		}

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
		PassParameters->InBuffer = GraphBuilder.CreateSRV(BufferB);
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


class FTextureMappingPS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FTextureMappingPS)
	SHADER_USE_PARAMETER_STRUCT(FTextureMappingPS, FGlobalShader)

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_STRUCT(FScreenPassTextureViewportParameters, OutViewPort)
		SHADER_PARAMETER_STRUCT(FScreenPassTextureViewportParameters, InViewPort)
		SHADER_PARAMETER_SAMPLER(SamplerState, sampler0)

		SHADER_PARAMETER_RDG_TEXTURE_SRV(Texture2D<float4>, InTextureToMap)
		// Output of Depth Alignment pipeline
		// with depth in R and UV map in GB
		SHADER_PARAMETER_RDG_TEXTURE_SRV(Texture2D<float4>, InAlignedDepth)

		RENDER_TARGET_BINDING_SLOTS()
	END_SHADER_PARAMETER_STRUCT()
};

IMPLEMENT_GLOBAL_SHADER(FTextureMappingPS, "/Plugin/CompositionUtils/DepthAlignment.usf", "TextureMappingPS", SF_Pixel);

void CompositionUtils::ExecuteTextureMappingPipeline(
	FRDGBuilder& GraphBuilder,
	FRDGTextureRef InTextureToMap,
	FRDGTextureRef InAlignedDepth,
	FRDGTextureRef OutTexture)
{
	check(IsInRenderingThread());

	// Create UV map
	CompositionUtils::AddPass<FTextureMappingPS>(
		GraphBuilder,
		RDG_EVENT_NAME("CompUtils.TextureMapping"),
		OutTexture,
		[&](auto PassParameters)
		{
			PassParameters->InTextureToMap = GraphBuilder.CreateSRV(InTextureToMap);
			PassParameters->InAlignedDepth = GraphBuilder.CreateSRV(InAlignedDepth);
		}
	);
}
