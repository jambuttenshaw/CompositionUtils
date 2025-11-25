#pragma once

#include "ScreenPass.h"
#include <functional>

#include "CompUtilsCameraData.h"


struct FDepthProcessingParametersProxy
{
	// Camera properties
	FMatrix44f InvProjectionMatrix; // for projecting points into view space

	// Relaxation parameters
	bool bEnableJacobiSteps;
	uint32 NumJacobiSteps;

	// Post-processing parameters
	bool bEnableFarClipping;
	float FarClipDistance;
	bool bEnableClippingPlane;
	FVector4f UserClippingPlane;
};

struct FDepthAlignmentParametersProxy
{
	FCompUtilsCameraIntrinsicData SourceCamera;
	FCompUtilsCameraIntrinsicData TargetCamera;

	// Extrinsic properties
	FMatrix44f SourceToTargetNodalOffset;

	uint32 HoleFillingBias = 0;
};

struct FDepthCalibrationParametersProxy
{
	FCompUtilsCameraIntrinsicData SourceCamera;

	uint32 CalibrationPointCount = 64;
	FVector4f CalibrationRulers{ 0.0f, 0.0f, 1.0f, 1.0f };

	// Calibration Visualization
	bool bShowPoints = true;
};

// Resources and parameters extracted from the scene render graph to be able to apply volumetric fog in composure
struct FVolumetricFogRequiredDataProxy
{
	// Resources
	TRefCountPtr<IPooledRenderTarget> IntegratedLightScatteringTexture;

	// Associated parameters
	float VolumetricFogStartDistance;
	FVector3f VolumetricFogInvGridSize;
	FVector3f VolumetricFogGridZParams;
	FVector2f VolumetricFogSVPosToVolumeUV;
	FVector2f VolumetricFogUVMax;
	float OneOverPreExposure;

	bool IsValid() const
	{
		bool bValid = true;
		bValid &= IntegratedLightScatteringTexture.IsValid();
		return bValid;
	}
};

struct FVolumetricsCompositionParametersProxy
{
	UTexture* CameraDepthTexture;

	const FVolumetricFogRequiredDataProxy* VolumetricFogData;

	bool IsValid() const
	{
		bool bValid = true;
		bValid &= CameraDepthTexture != nullptr;
		bValid &= VolumetricFogData != nullptr;
		bValid &= VolumetricFogData->IsValid();
		return bValid;
	}
};


struct FRelightingParametersProxy
{
	UTexture* CameraDepthTexture;
	UTexture* CameraNormalTexture;

	FLightSceneProxy* LightProxy;

	FTransform CameraTransform;

	float LightWeight;

	bool IsValid() const
	{
		bool bValid = true;
		bValid &= CameraDepthTexture != nullptr;
		bValid &= CameraNormalTexture != nullptr;
		bValid &= LightProxy != nullptr;
		return bValid;
	}
};


struct FCompUtilsCameraData;

namespace CompositionUtils
{

	void ExecuteDepthProcessingPipeline(
		FRDGBuilder& GraphBuilder,
		const FDepthProcessingParametersProxy& Parameters,
		FRDGTextureRef InTexture,
		FRDGTextureRef OutTexture
	);

	void VisualizeProcessedDepth(
		FRDGBuilder& GraphBuilder,
		FVector2f VisualizeRange, // [VisualizeRange.x, VisualizeRange.y] is mapped to [0,1]
		FRDGTextureRef ProcessedDepthTexture,
		FRDGTextureRef OutTexture
	);


	void ExecuteDepthAlignmentPipeline(
		FRDGBuilder& GraphBuilder,
		const FDepthAlignmentParametersProxy& Parameters,
		FRDGTextureRef InTexture,
		FRDGTextureRef OutTexture
	);


	void ExecuteDepthAlignmentCalibrationPipeline(
		FRDGBuilder& GraphBuilder,
		const FDepthCalibrationParametersProxy& Parameters,
		FRDGTextureRef InTexture,
		FRDGTextureRef OutTexture,
		FRHIGPUBufferReadback& CalibrationPointReadback
	);

	void VisualizeDepthAlignmentCalibrationPoints(
		FRDGBuilder& GraphBuilder,
		const FDepthCalibrationParametersProxy& Parameters,
		FRDGTextureRef InTexture,
		FRDGTextureRef OutTexture
	);


	void VisualizeNormalMap(
		FRDGBuilder& GraphBuilder,
		bool bWorldSpace,
		const FTransform& LocalToWorldTransform,
		FRDGTextureRef NormalMap,
		FRDGTextureRef OutTexture
	);
	

	void ExecuteVolumetricsCompositionPipeline(
		FRDGBuilder& GraphBuilder,
		const FVolumetricsCompositionParametersProxy& Parameters,
		FRDGTextureRef InTexture,
		FRDGTextureRef OutTexture
	);

	void ExecuteRelightingPipeline(
		FRDGBuilder& GraphBuilder,
		const FRelightingParametersProxy& Parameters,
		FRDGTextureRef InTexture,
		FRDGTextureRef OutTexture
	);


	void ExecuteAddCrosshairPipeline(
		FRDGBuilder& GraphBuilder,
		const FVector4f& Color,
		uint32 Width,
		uint32 Length,
		FRDGTextureRef InTexture,
		FRDGTextureRef OutTexture
	);


	// Helper functions:

	inline FRDGTextureRef CreateTextureFrom(FRDGBuilder& GraphBuilder, FRDGTextureRef InTex, const TCHAR* Name, float ScaleFactor = 1.0f)
	{
		FRDGTextureDesc Desc = InTex->Desc;
		Desc.ClearValue = FClearValueBinding(FLinearColor(0.0f, 0.0f, 0.0f));
		Desc.Format = PF_FloatRGBA;
		Desc.Extent.X = static_cast<int>(static_cast<float>(Desc.Extent.X) * ScaleFactor);
		Desc.Extent.Y = static_cast<int>(static_cast<float>(Desc.Extent.Y) * ScaleFactor);
		return GraphBuilder.CreateTexture(Desc, Name);
	}

	template <typename Shader, typename SamplerState = TStaticSamplerState<SF_Bilinear>>
	void AddPass(
		FRDGBuilder& GraphBuilder,
		FRDGEventName&& PassName,
		FRDGTextureRef RenderTarget,
		std::function<void(typename Shader::FParameters*)>&& SetPassParametersLambda,
		typename Shader::FPermutationDomain Permutation = TShaderPermutationDomain(),
		FIntRect OutRect = FIntRect(),
		FIntRect InRect = FIntRect()
	)
	{
		FScreenPassTextureViewport OutViewPort = OutRect.IsEmpty() ?
			FScreenPassTextureViewport{ RenderTarget->Desc.Extent } :
			FScreenPassTextureViewport{ RenderTarget->Desc.Extent, OutRect };
		FScreenPassTextureViewport InViewPort = InRect.IsEmpty() ?
			FScreenPassTextureViewport{ RenderTarget->Desc.Extent } :
			FScreenPassTextureViewport{ RenderTarget->Desc.Extent, InRect };

		typename Shader::FParameters* PassParameters = GraphBuilder.AllocParameters<typename Shader::FParameters>();
		PassParameters->OutViewPort = GetScreenPassTextureViewportParameters(OutViewPort);
		PassParameters->InViewPort = GetScreenPassTextureViewportParameters(InViewPort);
		PassParameters->sampler0 = SamplerState::GetRHI(); // bilinear clamped sampler

		SetPassParametersLambda(PassParameters);
		PassParameters->RenderTargets[0] = FRenderTargetBinding(RenderTarget, ERenderTargetLoadAction::ENoAction);

		const FGlobalShaderMap* ShaderMap = GetGlobalShaderMap(GMaxRHIFeatureLevel);
		TShaderMapRef<Shader> PixelShader(ShaderMap, Permutation);

		AddDrawScreenPass(
			GraphBuilder,
			std::move(PassName),
			FScreenPassViewInfo{ GMaxRHIFeatureLevel },
			OutViewPort,
			InViewPort,
			PixelShader,
			PassParameters
		);
	}


	// Misc helpers

	// Defined in CompUtilsDepthAlignmentPipeline.cpp
	TOptional<FPlane4f> CalculatePlaneOfBestFit(const TArray<FVector3f>& Points);

}
