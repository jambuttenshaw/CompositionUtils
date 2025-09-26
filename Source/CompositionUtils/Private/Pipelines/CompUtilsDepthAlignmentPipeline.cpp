#include "CompUtilsPipelines.h"
#include "RHIGPUReadback.h"

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

			PassParameters->PhysicalNDCToView = Parameters.AuxiliaryCameraData.NDCToViewMatrix;
			PassParameters->InvDeviceZToWorldZTransform = CreateInvDeviceZToWorldZTransform(static_cast<FMatrix>(Parameters.AuxiliaryCameraData.ViewToNDCMatrix));

			PassParameters->PhysicalToVirtualOffset = Parameters.AuxiliaryToPrimaryNodalOffset;
			PassParameters->VirtualViewToNDC = Parameters.VirtualCam_ViewToNDC;
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
			float Physical_TanHalfFOVX = FMath::Tan(0.5f * Parameters.AuxiliaryCameraData.HorizontalFOV);
			float Physical_TanHalfFOVY = FMath::Tan(0.5f * Parameters.AuxiliaryCameraData.VerticalFOV);
			float Virtual_TanHalfFOVX  = FMath::Tan(0.5f * Parameters.VirtualCam_HorizontalFOV);
			float Virtual_TanHalfFOVY = Virtual_TanHalfFOVX / Parameters.VirtualCam_AspectRatio;
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


class FSpawnPointsAndDeprojectCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FSpawnPointsAndDeprojectCS)
	SHADER_USE_PARAMETER_STRUCT(FSpawnPointsAndDeprojectCS, FGlobalShader)

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_RDG_TEXTURE_SRV(Texture2D<float4>, InDepthTexture)
		SHADER_PARAMETER_SAMPLER(SamplerState, DepthTextureSampler)

		SHADER_PARAMETER(FMatrix44f, PhysicalNDCToView)
		SHADER_PARAMETER(FVector4f, InvDeviceZToWorldZTransform)

		SHADER_PARAMETER(uint32, NumPoints)
		SHADER_PARAMETER(FVector4f, RulersMinAndMax)

		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<float3>, RWCalibrationPoints)
	END_SHADER_PARAMETER_STRUCT()

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZE_1D"), GetThreadGroupSize1D());
	}

	static uint32 GetThreadGroupSize1D() { return 32; }
};

IMPLEMENT_GLOBAL_SHADER(FSpawnPointsAndDeprojectCS, "/Plugin/CompositionUtils/DepthAlignment.usf", "SpawnPointsAndDeprojectCS", SF_Compute);


class FVisualizePointSpawningPS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FVisualizePointSpawningPS)
	SHADER_USE_PARAMETER_STRUCT(FVisualizePointSpawningPS, FGlobalShader)

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_STRUCT(FScreenPassTextureViewportParameters, OutViewPort)
		SHADER_PARAMETER_STRUCT(FScreenPassTextureViewportParameters, InViewPort)
		SHADER_PARAMETER_SAMPLER(SamplerState, sampler0)

		SHADER_PARAMETER_RDG_TEXTURE_SRV(Texture2D<float4>, InTex)

		SHADER_PARAMETER(uint32, NumPoints)
		SHADER_PARAMETER(FVector4f, RulersMinAndMax)

		SHADER_PARAMETER(uint32, bShowPoints)

		RENDER_TARGET_BINDING_SLOTS()
	END_SHADER_PARAMETER_STRUCT()
};

IMPLEMENT_GLOBAL_SHADER(FVisualizePointSpawningPS, "/Plugin/CompositionUtils/DepthAlignment.usf", "VisualizePointSpawningPS", SF_Pixel);


void CompositionUtils::ExecuteDepthAlignmentCalibrationPipeline(
	FRDGBuilder& GraphBuilder, 
	const FDepthAlignmentParametersProxy& Parameters, 
	FRDGTextureRef InTexture, 
	FRDGTextureRef OutTexture,
	FRHIGPUBufferReadback& CalibrationPointReadback)
{
	FRDGBufferRef CalibrationPointsBuffer = CreateStructuredBuffer(GraphBuilder, TEXT("CompositionUtils.DepthAlignment.CalibrationPoints"),
															 sizeof(FVector3f), Parameters.CalibrationPointCount, nullptr, 0);

	// Spawn points within rulers in screen space and reproject to 3D space
	// Then readback so that a plane of best fit can be estimated
	{
		FSpawnPointsAndDeprojectCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FSpawnPointsAndDeprojectCS::FParameters>();
		PassParameters->InDepthTexture = GraphBuilder.CreateSRV(InTexture);
		PassParameters->DepthTextureSampler = TStaticSamplerState<SF_Point>::GetRHI();

		PassParameters->PhysicalNDCToView = Parameters.AuxiliaryCameraData.NDCToViewMatrix;
		PassParameters->InvDeviceZToWorldZTransform = CreateInvDeviceZToWorldZTransform(static_cast<FMatrix>(Parameters.AuxiliaryCameraData.ViewToNDCMatrix));

		PassParameters->NumPoints = Parameters.CalibrationPointCount;
		PassParameters->RulersMinAndMax = Parameters.CalibrationRulers;

		PassParameters->RWCalibrationPoints = GraphBuilder.CreateUAV(CalibrationPointsBuffer);

		FGlobalShaderMap* ShaderMap = GetGlobalShaderMap(GMaxRHIFeatureLevel);
		TShaderMapRef<FSpawnPointsAndDeprojectCS> ComputeShader(ShaderMap);

		FIntVector GroupCount = FComputeShaderUtils::GetGroupCount(Parameters.CalibrationPointCount, FSpawnPointsAndDeprojectCS::GetThreadGroupSize1D());

		FComputeShaderUtils::AddPass(
			GraphBuilder,
			RDG_EVENT_NAME("CompUtils.SpawnPointsAndDeproject"),
			ERDGPassFlags::Compute,
			ComputeShader,
			PassParameters,
			GroupCount
		);
	}

	// Copy data into readback
	AddReadbackBufferPass(
		GraphBuilder, 
		RDG_EVENT_NAME("DepthAlignment.ReadbackCalibrationPoints"),
		CalibrationPointsBuffer,
		[&CalibrationPointReadback, CalibrationPointsBuffer](FRHICommandListImmediate& RHICmdList)
		{
			CalibrationPointReadback.EnqueueCopy(
				RHICmdList,
				CalibrationPointsBuffer->GetRHI()
			);
		});
}


void CompositionUtils::VisualizeDepthAlignmentCalibrationPoints(FRDGBuilder& GraphBuilder, const FDepthAlignmentParametersProxy& Parameters, FRDGTextureRef InTexture, FRDGTextureRef OutTexture)
{
	// Visualize rulers + points for helpful user feedback
	CompositionUtils::AddPass<FVisualizePointSpawningPS>(
		GraphBuilder,
		RDG_EVENT_NAME("CompUtils.Calibration.VisualizePointSpawning"),
		OutTexture,
		[&](auto PassParameters)
		{
			PassParameters->InTex = GraphBuilder.CreateSRV(InTexture);
			PassParameters->NumPoints = Parameters.CalibrationPointCount;
			PassParameters->RulersMinAndMax = Parameters.CalibrationRulers;

			PassParameters->bShowPoints = Parameters.bShowPoints ? 1 : 0;
		}
	);
}


// Translated to C++ from: https://www.ilikebigbits.com/2017_09_25_plane_from_points_2.html
TOptional<FPlane4f> CompositionUtils::CalculatePlaneOfBestFit(const TArray<FVector3f>& Points)
{
	uint32 N = Points.Num();
	if (N < 3)
	{
		return NullOpt;
	}

	FVector3f Sum{ 0.0f };
	for (const auto& Point : Points)
	{
		Sum += Point;
	}
	FVector3f Centroid = Sum / static_cast<float>(N);

	float XX{ 0.0f };
	float XY{ 0.0f };
	float XZ{ 0.0f };
	float YY{ 0.0f };
	float YZ{ 0.0f };
	float ZZ{ 0.0f };

	for (const auto& Point : Points)
	{
		FVector3f R = Point - Centroid;
		XX += R.X * R.X;
		XY += R.X * R.Y;
		XZ += R.X * R.Z;
		YY += R.Y * R.Y;
		YZ += R.Y * R.Z;
		ZZ += R.Z * R.Z;
	}

	XX /= static_cast<float>(N);
	XY /= static_cast<float>(N);
	XZ /= static_cast<float>(N);
	YY /= static_cast<float>(N);
	YZ /= static_cast<float>(N);
	ZZ /= static_cast<float>(N);

	FVector3f WeightedDir{ 0.0f };

	{
		float DetX = YY * ZZ - YZ * YZ;
		FVector3f AxisDir{
			DetX,
			XZ * YZ - XY * ZZ,
			XY * YZ - XZ * YY
		};
		float Weight = DetX * DetX;
		if ((WeightedDir | AxisDir) < 0.0f) { Weight = -Weight; }
		WeightedDir += AxisDir * Weight;
	}

	{
		float DetY = XX * ZZ - XZ * XZ;
		FVector3f AxisDir{
			XZ * YZ - XY * ZZ,
			DetY,
			XY * XZ - YZ * XX
		};
		float Weight = DetY * DetY;
		if ((WeightedDir | AxisDir) < 0.0f) { Weight = -Weight; }
		WeightedDir += AxisDir * Weight;
	}

	{
		float DetZ = XX * YY - XY * XY;
		FVector3f AxisDir{
			XY * YZ - XZ * YY,
			XY * XZ - YZ * XX,
			DetZ,
		};
		float Weight = DetZ * DetZ;
		if ((WeightedDir | AxisDir) < 0.0f) { Weight = -Weight; }
		WeightedDir += AxisDir * Weight;
	}

	FVector3f Normal = WeightedDir;
	if (Normal.Normalize())
	{
		return FPlane4f{ Centroid, Normal};
	}
	else
	{
		return NullOpt;
	}
}
