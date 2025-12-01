#pragma once

#include "CoreMinimal.h"
#include "Engine/TextureRenderTarget2D.h"


/**
 * Contains utilities and owns transient resources required to perform calibration
 */
class FCalibrator
{
public:
	bool RunCalibration(
		TObjectPtr<UTexture> Source,
		TObjectPtr<UTexture> Destination
	);

	TObjectPtr<UTexture> GetCalibratedSourceDebugView() const;
	TObjectPtr<UTexture> GetCalibratedDestinationDebugView() const;

	void InvalidateTransientResources();

private:

	static void CopyToIntermediate(TObjectPtr<UTexture> InTexture, TStrongObjectPtr<UTextureRenderTarget2D>& IntermediateTexture);

	// Helper to create an intermediate render target to hold data form InTexture
	static UTextureRenderTarget2D* CreateRenderTargetFrom(TObjectPtr<UTexture> InTexture, bool bClearRenderTarget);

private:
	// Transient resources required for calibration
	// Render targets required to easily read back texture data to CPU
	TStrongObjectPtr<UTextureRenderTarget2D> SourceIntermediateRT;
	TStrongObjectPtr<UTextureRenderTarget2D> DestinationIntermediateRT;

	// Debug views with checkerboard corners identified by OpenCV
	TStrongObjectPtr<UTexture2D> SourceDebugView;
	TStrongObjectPtr<UTexture2D> DestinationDebugView;
};
