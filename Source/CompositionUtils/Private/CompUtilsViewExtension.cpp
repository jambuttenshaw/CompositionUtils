#include "SlCompViewExtension.h"

#include "Composure/SlCompCaptureBase.h"
#include "ScreenPass.h"

#include "RenderGraphBuilder.h"
#include "SceneRendering.h"
#include "SlCompEngineSubsystem.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Pipelines/SlCompPipelines.h"


class FCameraFeedInjectionPS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FCameraFeedInjectionPS);
	SHADER_USE_PARAMETER_STRUCT(FCameraFeedInjectionPS, FGlobalShader)

	class FColorSourceDepthCamera : SHADER_PERMUTATION_BOOL("COlOR_SOURCE_DEPTH_CAMERA");
	using FPermutationDomain = TShaderPermutationDomain<FColorSourceDepthCamera>;

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters,)
		SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, View)

		SHADER_PARAMETER_STRUCT(FScreenPassTextureViewportParameters, InputViewPort)
		SHADER_PARAMETER_STRUCT(FScreenPassTextureViewportParameters, OutputViewPort)

		SHADER_PARAMETER_TEXTURE(Texture2D<float4>, CameraColorTexture) // Not RDG resources
		SHADER_PARAMETER_TEXTURE(Texture2D<float4>, CameraDepthTexture) 
		SHADER_PARAMETER_TEXTURE(Texture2D<float4>, CameraNormalsTexture)

		SHADER_PARAMETER_RDG_TEXTURE_SRV(Texture2D<float2>, ReprojectionUVMap)

		SHADER_PARAMETER(FMatrix44f, VirtualCameraLocalToWorld)
		SHADER_PARAMETER(FMatrix44f, VirtualCameraViewToNDC)
		SHADER_PARAMETER(FMatrix44f, VirtualCameraNDCToView)

		SHADER_PARAMETER(FMatrix44f, DepthCameraViewToNDC)
		SHADER_PARAMETER(FMatrix44f, DepthCameraNDCToView)

		SHADER_PARAMETER(float, AlbedoMultiplier)
		SHADER_PARAMETER(float, AmbientMultiplier)

		SHADER_PARAMETER(float, RoughnessOverride)
		SHADER_PARAMETER(float, SpecularOverride)

		RENDER_TARGET_BINDING_SLOTS()
	END_SHADER_PARAMETER_STRUCT()

	static void ModifyCompilationEnvironment(
		const FGlobalShaderPermutationParameters& Parameters,
		FShaderCompilerEnvironment& OutEnvironment
	)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);

		// We need to set these so that the shader generation utils will set up the GBuffer writing defines
		// TODO: This will probably cause all sorts of problems if also using substrate
		// TODO: because the render target layout is different
		OutEnvironment.SetDefine(TEXT("IS_BASE_PASS"), true);
		OutEnvironment.SetDefine(TEXT("MATERIALBLENDING_SOLID"), true);
		OutEnvironment.SetDefine(TEXT("MATERIAL_SHADINGMODEL_DEFAULT_LIT"), true);
	}
};

IMPLEMENT_GLOBAL_SHADER(FCameraFeedInjectionPS, "/Plugin/StereolabsCompositing/CameraFeedInjection.usf", "InjectCameraFeedPS", SF_Pixel)


FSlCompViewExtension::FSlCompViewExtension(AStereolabsCompositingCaptureBase* Owner)
	: CaptureActor(Owner)
{
	check(Owner);
}


void FSlCompViewExtension::PostRenderBasePassDeferred_RenderThread(FRDGBuilder& GraphBuilder, FSceneView& InView, const FRenderTargetBindingSlots& RenderTargets, TRDGUniformBufferRef<FSceneTextureUniformParameters> SceneTextures)
{
	check(InView.bIsSceneCapture && InView.bIsViewInfo && CaptureActor.IsValid());

	if (CaptureActor->bInjectionMode)
	{
		InjectCameraFeed(GraphBuilder, InView);
	}
}

void FSlCompViewExtension::PostRenderView_RenderThread(FRDGBuilder& GraphBuilder, FSceneView& View)
{
	check(View.bIsSceneCapture && View.bIsViewInfo && CaptureActor.IsValid());

	if (!CaptureActor->bInjectionMode || CaptureActor->bExtractVolumetricFogInInjectionMode)
	{
		ExtractVolumetricFog(GraphBuilder, View);
	}
}


void FSlCompViewExtension::InjectCameraFeed(FRDGBuilder& GraphBuilder, FSceneView& View) const
{
	check(View.bIsSceneCapture && View.bIsViewInfo && CaptureActor.IsValid());
	FViewInfo& ViewInfo = static_cast<FViewInfo&>(View);

	// Get camera images and insert them into GBuffer
	// Some of these textures may be nullptr, must check for that later
	const FStereolabsCameraTexturesProxy& CameraTextures = CaptureActor->GetCameraTextures_RenderThread();

	bool bAllValid = CameraTextures.ColorTexture != nullptr
		&& CameraTextures.DepthTexture != nullptr
		&& CameraTextures.NormalsTexture != nullptr;

	if (!bAllValid)
	{
		return;
	}

	FRDGTextureRef ReprojectionUVMap;
	{
		FMinimalViewInfo VirtualCameraView;
		if (CaptureActor->TargetCameraActorPtr.IsValid())
		{
			CaptureActor->TargetCameraActorPtr->GetCameraComponent()->GetCameraView(0.0f, VirtualCameraView);
		}
		else
		{
			CaptureActor->SceneCaptureComponent2D->GetCameraView(0.0f, VirtualCameraView);
		}
		FIntPoint TextureExtent = {
			static_cast<int32>(CameraTextures.DepthTexture->GetResource()->GetSizeX()),
			static_cast<int32>(CameraTextures.DepthTexture->GetResource()->GetSizeY())
		};

		ReprojectionUVMap = StereolabsCompositing::CreateReprojectionUVMap(
			GraphBuilder,
			VirtualCameraView,
			TextureExtent,
			CaptureActor->bDisableReprojectionUVMap
		);
	}

	{
		FCameraFeedInjectionPS::FParameters* PassParameters = GraphBuilder.AllocParameters<FCameraFeedInjectionPS::FParameters>();

		PassParameters->View = View.ViewUniformBuffer;

		PassParameters->CameraColorTexture = CameraTextures.ColorTexture->GetResource()->TextureRHI;
		PassParameters->CameraDepthTexture = CameraTextures.DepthTexture->GetResource()->TextureRHI;
		PassParameters->CameraNormalsTexture = CameraTextures.NormalsTexture->GetResource()->TextureRHI;

		PassParameters->ReprojectionUVMap = GraphBuilder.CreateSRV(ReprojectionUVMap);

		// Get virtual camera properties (which will match the film camera if one is in use)
		{
			FMinimalViewInfo CameraView;
			CaptureActor->SceneCaptureComponent2D->GetCameraView(0.0f, CameraView);

			FTransform CameraTransform;
			CameraTransform.SetRotation(CameraView.Rotation.Quaternion());
			CameraTransform.SetTranslation(CameraView.Location);
			PassParameters->VirtualCameraLocalToWorld = static_cast<FMatrix44f>(CameraTransform.ToMatrixNoScale());

			FMatrix CameraProjectionMatrix = CameraView.CalculateProjectionMatrix();
			PassParameters->VirtualCameraViewToNDC = static_cast<FMatrix44f>(CameraProjectionMatrix);
			PassParameters->VirtualCameraNDCToView = static_cast<FMatrix44f>(CameraProjectionMatrix.Inverse());
		}

		// Get physical depth camera properties
		{
			USlCompEngineSubsystem* Subsystem = GEngine->GetEngineSubsystem<USlCompEngineSubsystem>();
			PassParameters->DepthCameraViewToNDC = static_cast<FMatrix44f>(Subsystem->GetProjectionMatrix());
			PassParameters->DepthCameraNDCToView = static_cast<FMatrix44f>(Subsystem->GetInvProjectionMatrix());
		}

		PassParameters->AmbientMultiplier = CaptureActor->AmbientMultiplier;
		PassParameters->AlbedoMultiplier = CaptureActor->AlbedoMultiplier;

		PassParameters->RoughnessOverride = CaptureActor->RoughnessOverride;
		PassParameters->SpecularOverride = CaptureActor->SpecularOverride;

		// Get GBuffer
		const FSceneTextures& SceneTexturesData = ViewInfo.GetSceneTextures();
		TStaticArray<FTextureRenderTargetBinding, MaxSimultaneousRenderTargets> RenderTargetTextures;
		uint32 RenderTargetTextureCount = SceneTexturesData.GetGBufferRenderTargets(RenderTargetTextures);
		TArrayView<FTextureRenderTargetBinding> RenderTargetTexturesView(RenderTargetTextures.GetData(), RenderTargetTextureCount);

		FRDGTextureRef DepthTexture = GetIfProduced(SceneTexturesData.Depth.Target);
		check(DepthTexture);

		PassParameters->RenderTargets = GetRenderTargetBindings(ERenderTargetLoadAction::ELoad, RenderTargetTexturesView);
		PassParameters->RenderTargets.DepthStencil = FDepthStencilBinding(
			DepthTexture, ERenderTargetLoadAction::ELoad,
			ERenderTargetLoadAction::ENoAction, FExclusiveDepthStencil::DepthWrite_StencilNop 
		);

		FGlobalShaderMap* ShaderMap = GetGlobalShaderMap(View.FeatureLevel);
		TShaderMapRef<FScreenPassVS> VertexShader(ShaderMap);

		FCameraFeedInjectionPS::FPermutationDomain Permutation;
		Permutation.Set<FCameraFeedInjectionPS::FColorSourceDepthCamera>(true);
		TShaderMapRef<FCameraFeedInjectionPS> PixelShader(ShaderMap, Permutation);

		FScreenPassTextureViewport ViewPort(ViewInfo.ViewRect.Size());

		PassParameters->InputViewPort = GetScreenPassTextureViewportParameters(ViewPort);
		PassParameters->OutputViewPort = GetScreenPassTextureViewportParameters(ViewPort);

		using FCompositionDepthStencilState = TStaticDepthStencilState<true, CF_Greater>;

		AddDrawScreenPass(
			GraphBuilder,
			RDG_EVENT_NAME("StereolabsCameraFeedInjection"),
			View,
			ViewPort,
			ViewPort,
			VertexShader,
			PixelShader,
			FCompositionDepthStencilState::GetRHI(),
			PassParameters
		);
	}
}
