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

		SHADER_PARAMETER(FMatrix44f, PhysicalNDCToView)
		SHADER_PARAMETER(FVector4f, InvDeviceZToWorldZTransform)

		SHADER_PARAMETER(FMatrix44f, PhysicalToVirtualOffset)
		SHADER_PARAMETER(FMatrix44f, VirtualViewToNDC)

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


/**
 * Utility function borrowed from SceneView.cpp
 */
static FVector4f CreateInvDeviceZToWorldZTransform(const FMatrix& ProjMatrix)
{
	// The perspective depth projection comes from the the following projection matrix:
	//
	// | 1  0  0  0 |
	// | 0  1  0  0 |
	// | 0  0  A  1 |
	// | 0  0  B  0 |
	//
	// Z' = (Z * A + B) / Z
	// Z' = A + B / Z
	//
	// So to get Z from Z' is just:
	// Z = B / (Z' - A)
	//
	// Note a reversed Z projection matrix will have A=0.
	//
	// Done in shader as:
	// Z = 1 / (Z' * C1 - C2)   --- Where C1 = 1/B, C2 = A/B
	//

	float DepthMul = (float)ProjMatrix.M[2][2];
	float DepthAdd = (float)ProjMatrix.M[3][2];

	if (DepthAdd == 0.f)
	{
		// Avoid dividing by 0 in this case
		DepthAdd = 0.00000001f;
	}

	if (ProjMatrix.M[3][3] < 1.0f) // Perspective projection
	{
		float SubtractValue = DepthMul / DepthAdd;

		// Subtract a tiny number to avoid divide by 0 errors in the shader when a very far distance is decided from the depth buffer.
		// This fixes fog not being applied to the black background in the editor.
		SubtractValue -= 0.00000001f;

		return FVector4f(
			0.0f,
			0.0f,
			1.0f / DepthAdd,
			SubtractValue
		);
	}
	else
	{
		return FVector4f(
			(float)(1.0f / ProjMatrix.M[2][2]),
			(float)(-ProjMatrix.M[3][2] / ProjMatrix.M[2][2] + 1.0f),
			0.0f,
			1.0f
		);
	}
}


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
		FRDGTextureDesc::Create2D(Extent, PF_G32R32F, FClearValueBinding{{-1, -1, -1, -1}}, TexCreate_RenderTargetable | TexCreate_ShaderResource),
		TEXT("CompUtils.DepthAlignment.UVMap")
	);
	AddClearRenderTargetPass(GraphBuilder, UVMap);

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

	// Create UV map
	CompositionUtils::AddPass<FCalculateUVMapPS>(
		GraphBuilder,
		RDG_EVENT_NAME("CompUtils.CalculateUVMap"),
		UVMap,
		[&](auto PassParameters)
		{
			PassParameters->InTex = GraphBuilder.CreateSRV(InTexture);

			PassParameters->PhysicalNDCToView = Parameters.AuxiliaryCameraData.NDCToViewMatrix;
			PassParameters->InvDeviceZToWorldZTransform = CreateInvDeviceZToWorldZTransform(static_cast<FMatrix>(Parameters.AuxiliaryCameraData.ViewToNDCMatrix));

			PassParameters->PhysicalToVirtualOffset = FMatrix44f::Identity;
			PassParameters->VirtualViewToNDC = Parameters.VirtualCam_ViewToNDC;
		}
	);

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

		// Calculate how big a patch size is required to avoid holes
		{
			float Physical_TanHalfFOVX = FMath::Tan(0.5f * Parameters.AuxiliaryCameraData.HorizontalFOV);
			float Physical_TanHalfFOVY = FMath::Tan(0.5f * Parameters.AuxiliaryCameraData.VerticalFOV);
			float Virtual_TanHalfFOVX  = FMath::Tan(0.5f * Parameters.VirtualCam_HorizontalFOV);
			float Virtual_TanHalfFOVY = Virtual_TanHalfFOVX / Parameters.VirtualCam_AspectRatio;
			FIntPoint PatchSize = {
				FMath::CeilToInt(Physical_TanHalfFOVX / Virtual_TanHalfFOVX),
				FMath::CeilToInt(Physical_TanHalfFOVY / Virtual_TanHalfFOVY)
			};

			PassParameters->PatchSize = FUintVector2{
				static_cast<uint32>(FMath::Clamp(PatchSize.X, 1, 16)),
				static_cast<uint32>(FMath::Clamp(PatchSize.Y, 1, 16))
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
