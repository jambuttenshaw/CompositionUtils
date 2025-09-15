#pragma once

#include "SceneViewExtension.h"


class AStereolabsCompositingCaptureBase;


// Inherits from ISceneViewExtension instead of FSceneViewExtensionBase because
// we do not want to create this view extension through FSceneViewExtensions
// We only want to create them for ASlCompCaptureBase actors
class FSlCompViewExtension : public ISceneViewExtension
{
public:
	FSlCompViewExtension(AStereolabsCompositingCaptureBase* Owner);

	//~ Begin ISceneViewExtension Interface

	virtual void SetupViewFamily(FSceneViewFamily& InViewFamily) override {};
	virtual void SetupView(FSceneViewFamily& InViewFamily, FSceneView& InView) override {};
	virtual void BeginRenderViewFamily(FSceneViewFamily& InViewFamily) override {};

	// Camera feed injection is performed after base pass and before lighting pass
	virtual void PostRenderBasePassDeferred_RenderThread(
		FRDGBuilder& GraphBuilder,
		FSceneView& InView, 
		const FRenderTargetBindingSlots& RenderTargets, 
		TRDGUniformBufferRef<FSceneTextureUniformParameters> SceneTextures) override;

	// Any resource extraction required by non-injection-based composition methods should execute after the view has been rendered
	virtual void PostRenderView_RenderThread(FRDGBuilder& GraphBuilder, FSceneView& InView) override;

	//~ End ISceneViewExtension Interface

private:


	void InjectCameraFeed(FRDGBuilder& GraphBuilder, FSceneView& View) const;
	// Implemented in SlCompVolumetricFogExtraction.cpp
	void ExtractVolumetricFog(FRDGBuilder& GraphBuilder, FSceneView& View) const;

private:
	// Should always be valid through the lifetime of this object or something has gone wrong
	TWeakObjectPtr<AStereolabsCompositingCaptureBase> CaptureActor;
};
