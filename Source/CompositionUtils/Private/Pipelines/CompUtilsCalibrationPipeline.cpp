#include "CompUtilsPipelines.h"
#include "RHIGPUReadback.h"


class FSpawnPointsAndDeprojectCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FSpawnPointsAndDeprojectCS)
	SHADER_USE_PARAMETER_STRUCT(FSpawnPointsAndDeprojectCS, FGlobalShader)

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_RDG_TEXTURE_SRV(Texture2D<float4>, InDepthTexture)
		SHADER_PARAMETER_SAMPLER(SamplerState, DepthTextureSampler)

		SHADER_PARAMETER(FMatrix44f, SourceNDCToView)

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

IMPLEMENT_GLOBAL_SHADER(FSpawnPointsAndDeprojectCS, "/Plugin/CompositionUtils/DepthCalibration.usf", "SpawnPointsAndDeprojectCS", SF_Compute);


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

IMPLEMENT_GLOBAL_SHADER(FVisualizePointSpawningPS, "/Plugin/CompositionUtils/DepthCalibration.usf", "VisualizePointSpawningPS", SF_Pixel);


void CompositionUtils::ExecuteDepthAlignmentCalibrationPipeline(
	FRDGBuilder& GraphBuilder,
	const FDepthCalibrationParametersProxy& Parameters,
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

		PassParameters->SourceNDCToView = Parameters.SourceCamera.NDCToView;

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


void CompositionUtils::VisualizeDepthAlignmentCalibrationPoints(FRDGBuilder& GraphBuilder, const FDepthCalibrationParametersProxy& Parameters, FRDGTextureRef InTexture, FRDGTextureRef OutTexture)
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
		return FPlane4f{ Centroid, Normal };
	}
	else
	{
		return NullOpt;
	}
}
