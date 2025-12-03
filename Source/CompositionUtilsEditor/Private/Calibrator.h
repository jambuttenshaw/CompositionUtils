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
	// Resets state to do with the calibration process, but doesn't free transient resources.
	// Use this restart progressive calibration
	void RestartCalibration();
	// Releases transient resources so that they will be recreated next time calibration is performed.
	// Use this only when the texture sizes of the input feeds change.
	void ResetTransientResources();

	ECalibrationResult RunCalibration(
		TObjectPtr<UReprojectionCalibrationTargetBase> Source,
		TObjectPtr<UReprojectionCalibrationTargetBase> Destination,
		FIntPoint CheckerboardDimensions,
		float CheckerboardSize,
		FTransform& OutSourceToDestination
	);

	TObjectPtr<UTexture> GetCalibratedSourceDebugView() const;
	TObjectPtr<UTexture> GetCalibratedDestinationDebugView() const;

	inline int32 GetNumSamples() const { return NumSamples; }
	inline double GetAvgSourceError() const { return NumSamples == 0 ? 0 : SourceErrorSum / static_cast<double>(NumSamples); }
	inline double GetAvgDestError() const { return NumSamples == 0 ? 0 : DestinationErrorSum / static_cast<double>(NumSamples); }

	inline double GetCurrentSourceError() const { return CurrentSourceError; }
	inline double GetCurrentDestError() const { return CurrentDestinationError; }
	inline const FTransform& GetCurrentCalibratedTransform() const { return CurrentCalibratedTransform; }

	static FText GetErrorTextForResult(ECalibrationResult Result);

private:
	ECalibrationResult RunCalibrationImpl(
		TObjectPtr<UReprojectionCalibrationTargetBase> Source,
		TObjectPtr<UReprojectionCalibrationTargetBase> Destination,
		FIntPoint CheckerboardDimensions,
		float CheckerboardSize,
		FTransform& OutSourceToDestination
	);

	static TObjectPtr<UTexture> GetDebugView(const FTransientResources& Resources);

	static ECalibrationResult FindCheckerboardCorners(
		FIntPoint CheckerboardDimensions,
		TObjectPtr<UTexture> InTexture,
		FTransientResources& Resources,
		TArray<FVector2f>& OutCorners
	);
	static void CopyToIntermediate(TObjectPtr<UTexture> InTexture, TStrongObjectPtr<UTextureRenderTarget2D>& IntermediateTexture);

	// Helper to create an intermediate render target to hold data form InTexture
	static UTextureRenderTarget2D* CreateRenderTargetFrom(TObjectPtr<UTexture> InTexture, bool bClearRenderTarget);

private:
	// Transient resources are stored as members to avoid re-allocation every run
	TStaticArray<FTransientResources, Resources_Count> TransientResources;

	// State for progressive calibration - where the calibration is refined over multiple runs
	int32 NumSamples = 0;
	double WeightSum = 0;
	double SourceErrorSum = 0;
	double DestinationErrorSum = 0;

	FQuat AccumulatedRotation = FQuat::Identity;
	FVector AccumulatedTranslation = FVector::ZeroVector;

	// The properties of the most recent calibration run
	double CurrentSourceError = 0;
	double CurrentDestinationError = 0;
	FTransform CurrentCalibratedTransform = FTransform::Identity;
};
