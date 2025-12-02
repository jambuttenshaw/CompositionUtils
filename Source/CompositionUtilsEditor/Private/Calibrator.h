#pragma once

#include "CoreMinimal.h"
#include "Engine/TextureRenderTarget2D.h"

class UReprojectionCalibration;
class UReprojectionCalibrationTargetBase;

/**
 * Contains utilities and owns transient resources required to perform calibration
 */
class FCalibrator
{
	// Transient resources required for calibration
	struct FTransientResources
	{
		// Render targets used to easily read back texture data to CPU
		TStrongObjectPtr<UTextureRenderTarget2D> Intermediate;
		// Debug texture visualizes corners to give feedback to user
		TStrongObjectPtr<UTexture2D> DebugView;
		bool bValidDebugView = false;

		void ReleaseAll()
		{
			Intermediate.Reset();
			DebugView.Reset();
			bValidDebugView = false;
		}
	};

	enum ResourceSets
	{
		Resources_Source = 0,
		Resources_Destination,
		Resources_Count
	};

public:
	enum class ECalibrationResult
	{
		Success = 0,
		Error_NoOpenCV,
		Error_InvalidParams,
		Error_MissingSourceOrDestination,
		Error_MissingIntrinsics,
		Error_ReadTextureFailure,
		Error_IdentifyCheckerboardFailure,
		Error_PointCountMismatch,
		Error_DrawCheckerboardFailure,
		Error_SolvePoseFailure
	};

public:
	// Resets state to do with the asset or calibration process, but doesn't free transient resources.
	// Use this when any asset state or calibration parameters are changed.
	void ResetCalibrationState(TObjectPtr<UReprojectionCalibration> InAsset);
	// Releases transient resources so that they will be recreated next time calibration is performed.
	// Use this only when the texture sizes of the input feeds change.
	void ResetTransientResources();

	// Ensure ResetCalibrationState has been called at least once before running calibration
	ECalibrationResult RunCalibration(
		TObjectPtr<UReprojectionCalibrationTargetBase> Source,
		TObjectPtr<UReprojectionCalibrationTargetBase> Destination,
		FTransform& OutSourceToDestination
	);

	TObjectPtr<UTexture> GetCalibratedSourceDebugView() const;
	TObjectPtr<UTexture> GetCalibratedDestinationDebugView() const;


	static FText GetErrorTextForResult(ECalibrationResult Result);

private:
	static TObjectPtr<UTexture> GetDebugView(const FTransientResources& Resources);

	static ECalibrationResult FindCheckerboardCorners(
		TObjectPtr<UReprojectionCalibration> Asset,
		TObjectPtr<UTexture> InTexture,
		FTransientResources& Resources,
		TArray<FVector2f>& OutCorners
	);
	static void CopyToIntermediate(TObjectPtr<UTexture> InTexture, TStrongObjectPtr<UTextureRenderTarget2D>& IntermediateTexture);

	// Helper to create an intermediate render target to hold data form InTexture
	static UTextureRenderTarget2D* CreateRenderTargetFrom(TObjectPtr<UTexture> InTexture, bool bClearRenderTarget);

private:
	TStaticArray<FTransientResources, Resources_Count> TransientResources;

	TObjectPtr<UReprojectionCalibration> Asset;

	TArray<FVector2f> SourceCorners;
	TArray<FVector2f> DestinationCorners;

	TArray<FVector> ObjectPoints;
};
