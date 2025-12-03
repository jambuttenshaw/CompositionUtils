#include "Calibrator.h"

#include "CompositionUtilsEditor.h"
#include "MediaTexture.h"
#include "RenderGraphBuilder.h"
#include "RenderGraphUtils.h"

#include "ReprojectionCalibration.h"

#include "OpenCVHelper.h"

#define LOCTEXT_NAMESPACE "FCompositionUtilsEditorModule"


void FCalibrator::RestartCalibration()
{
	NumSamples = 0;
	WeightSum = 0;
	SourceErrorSum = 0;
	DestinationErrorSum = 0;

	AccumulatedRotation = FQuat::Identity;
	AccumulatedTranslation = FVector::ZeroVector;

	CurrentSourceError = 0;
	CurrentDestinationError = 0;
	CurrentSampleWeight = 0;
	CurrentCalibratedTransform.SetIdentity();

	// It is helpful to clear any debug views from previous runs when calibration is restarted
	for (auto& Resources : TransientResources)
	{
		Resources.bValidDebugView = false;
	}
}

void FCalibrator::ResetTransientResources()
{
	for (auto& Resources : TransientResources)
	{
		Resources.ReleaseAll();
	}
}

FCalibrator::ECalibrationResult FCalibrator::RunCalibration(
	TObjectPtr<UReprojectionCalibrationTargetBase> Source,
	TObjectPtr<UReprojectionCalibrationTargetBase> Destination,
	FIntPoint CheckerboardDimensions,
	float CheckerboardSize,
	FTransform& OutSourceToDestination)
{
	for (auto& Resources : TransientResources)
	{
		Resources.bValidDebugView = false;
	}

	ECalibrationResult Result = RunCalibrationImpl(
		Source,
		Destination,
		CheckerboardDimensions,
		CheckerboardSize,
		OutSourceToDestination);

	if (Result != ECalibrationResult::Success)
	{
		// To make it less confusing, either show both successful debug images or neither
		for (auto& Resources : TransientResources)
		{
			Resources.bValidDebugView = false;
		}
	}

	return Result;
}

FCalibrator::ECalibrationResult FCalibrator::RunCalibrationImpl(
	TObjectPtr<UReprojectionCalibrationTargetBase> Source,
	TObjectPtr<UReprojectionCalibrationTargetBase> Destination,
	FIntPoint CheckerboardDimensions,
	float CheckerboardSize,
	FTransform& OutSourceToDestination)
{
	// Calibration relies on OpenCV to run
#if WITH_OPENCV

	// Validate inputs
	if (!(CheckerboardDimensions.X > 0 && CheckerboardDimensions.Y > 0 && CheckerboardSize > 0.0f))	
	{
		return ECalibrationResult::Error_InvalidParams;
	}

	if (!Source || !Destination || !Source->GetTexture() || !Destination->GetTexture())
	{
		return ECalibrationResult::Error_MissingSourceOrDestination;
	}

	FCompUtilsCameraIntrinsicData SourceIntrinsics, DestinationIntrinsics;
	if (!Source->GetCameraIntrinsicData(SourceIntrinsics) || !Destination->GetCameraIntrinsicData(DestinationIntrinsics))
	{
		return ECalibrationResult::Error_MissingIntrinsics;
	}

	// Build object space points (this is done every run in case checkerboard has changed)
	// TODO: Notes on coordinate space
	TArray<FVector> ObjectPoints;
	ObjectPoints.Reserve(CheckerboardDimensions.X * CheckerboardDimensions.Y);
	for (int32 Y = 0; Y < CheckerboardDimensions.Y; Y++)
	{
		for (int32 X = 0; X < CheckerboardDimensions.X; X++)
		{
			ObjectPoints.Add(FVector{ X * CheckerboardSize, Y * CheckerboardSize, 0.0 });
		}
	}

	// First, find checkerboard corners for the current pair of images
	TArray<FVector2f> SourceCorners, DestinationCorners;
	ECalibrationResult Result = FindCheckerboardCorners(CheckerboardDimensions, Source->GetTexture(), TransientResources[Resources_Source], SourceCorners);
	if (Result != ECalibrationResult::Success)
	{
		return Result;
	}

	Result = FindCheckerboardCorners(CheckerboardDimensions, Destination->GetTexture(), TransientResources[Resources_Destination], DestinationCorners);
	if (Result != ECalibrationResult::Success)
	{
		return Result;
	}

	if (SourceCorners.Num() != ObjectPoints.Num() || DestinationCorners.Num() != ObjectPoints.Num())
	{
		return ECalibrationResult::Error_PointCountMismatch;
	}

	// With checkerboard corners found, attempt to solve for the pose of each camera
	FTransform SourceCameraPose, DestinationCameraPose;

	if (!FOpenCVHelper::SolvePnP(
		ObjectPoints,
		SourceCorners,
		SourceIntrinsics.FocalLength,
		SourceIntrinsics.ImageCenter,
		SourceIntrinsics.DistortionParams,
		SourceCameraPose))
	{
		return ECalibrationResult::Error_SolvePoseFailure;
	}

	if (!FOpenCVHelper::SolvePnP(
		ObjectPoints,
		DestinationCorners,
		DestinationIntrinsics.FocalLength, 
		DestinationIntrinsics.ImageCenter, 
		DestinationIntrinsics.DistortionParams,
		DestinationCameraPose))
	{
		return ECalibrationResult::Error_SolvePoseFailure;
	}

	// Find the transform to get from Source to Destination
	FQuat SourceToDestinationRotation = SourceCameraPose.GetRotation() * DestinationCameraPose.GetRotation().Inverse();
	FVector SourceToDestinationTranslation = SourceCameraPose.GetTranslation() - DestinationCameraPose.GetTranslation();

	// Weight and accumulate output transform
	CurrentSourceError = FOpenCVHelper::ComputeReprojectionError(ObjectPoints, SourceCorners, SourceIntrinsics.FocalLength, SourceIntrinsics.ImageCenter, SourceCameraPose);
	CurrentDestinationError = FOpenCVHelper::ComputeReprojectionError(ObjectPoints, DestinationCorners, DestinationIntrinsics.FocalLength, DestinationIntrinsics.ImageCenter, DestinationCameraPose);

	// Error should never be 0 in reality
	double Weight = 1.0 / FMath::Max(UE_KINDA_SMALL_NUMBER, CurrentSourceError + CurrentDestinationError);
	WeightSum += Weight;
	// Normalize weight across samples
	CurrentSampleWeight = Weight / WeightSum;

	AccumulatedRotation = FQuat::Slerp(AccumulatedRotation, SourceToDestinationRotation, CurrentSampleWeight);
	AccumulatedTranslation = FMath::Lerp(AccumulatedTranslation, SourceToDestinationTranslation, CurrentSampleWeight);

	NumSamples++;
	SourceErrorSum += CurrentSourceError;
	DestinationErrorSum += CurrentDestinationError;

	CurrentCalibratedTransform.SetIdentity();
	CurrentCalibratedTransform.SetRotation(AccumulatedRotation);
	CurrentCalibratedTransform.SetTranslation(AccumulatedTranslation);

	OutSourceToDestination = CurrentCalibratedTransform;

	return ECalibrationResult::Success;
#else
	// Calibration can never succeed without OpenCV
	return ECalibrationResult::Error_NoOpenCV;
#endif
}

TObjectPtr<UTexture> FCalibrator::GetCalibratedSourceDebugView() const
{
	return GetDebugView(TransientResources[Resources_Source]);
}

TObjectPtr<UTexture> FCalibrator::GetCalibratedDestinationDebugView() const
{
	return GetDebugView(TransientResources[Resources_Destination]);
}

TObjectPtr<UTexture> FCalibrator::GetDebugView(const FTransientResources& Resources)
{
	if (Resources.bValidDebugView)
	{
		check(Resources.DebugView.IsValid() && Resources.DebugView->GetResource());
		return Resources.DebugView.Get();
	}
	return nullptr;
}


FCalibrator::ECalibrationResult FCalibrator::FindCheckerboardCorners(
	FIntPoint CheckerboardDimensions,
	TObjectPtr<UTexture> InTexture, 
	FTransientResources& Resources, 
	TArray<FVector2f>& OutCorners)
{
	// Copying to an intermediate texture handles format conversion and allows us to use UTextureRenderTarget2D's API here
	CopyToIntermediate(InTexture, Resources.Intermediate);

	TArray<FColor> ImageData;
	if (!Resources.Intermediate->GameThread_GetRenderTargetResource()->ReadPixels(ImageData))
	{
		return ECalibrationResult::Error_ReadTextureFailure;
	}

	FIntPoint SourceImageSize{
		static_cast<int32>(InTexture->GetSurfaceWidth()),
		static_cast<int32>(InTexture->GetSurfaceHeight())
	};

	if (!FOpenCVHelper::IdentifyCheckerboard(ImageData, SourceImageSize, CheckerboardDimensions, OutCorners))
	{
		return ECalibrationResult::Error_IdentifyCheckerboardFailure;
	}

	if (!Resources.DebugView
	 || !Resources.DebugView->GetPlatformData()
	 ||  Resources.DebugView->GetPlatformData()->Mips.IsEmpty())
	{
		Resources.DebugView.Reset(
			UTexture2D::CreateTransient(SourceImageSize.X, SourceImageSize.Y, Resources.Intermediate->GetFormat(), NAME_None, {})
		);
		Resources.DebugView->SRGB = false;
	}

	{
		auto& Mip = Resources.DebugView->GetPlatformData()->Mips[0];

		TConstArrayView64<uint8> ImageDataView(reinterpret_cast<uint8*>(ImageData.GetData()), ImageData.Num() * ImageData.GetTypeSize());

		void* DestImageData = Mip.BulkData.Lock(LOCK_READ_WRITE);
		FMemory::Memcpy(DestImageData, ImageDataView.GetData(), ImageDataView.NumBytes());
		Mip.BulkData.Unlock();

		Resources.DebugView->UpdateResource();
	}

	if (!FOpenCVHelper::DrawCheckerboardCorners(OutCorners, CheckerboardDimensions, Resources.DebugView.Get()))
	{
		return ECalibrationResult::Error_DrawCheckerboardFailure;
	}

	Resources.bValidDebugView = true;

	return ECalibrationResult::Success;
}

void FCalibrator::CopyToIntermediate(TObjectPtr<UTexture> InTexture, TStrongObjectPtr<UTextureRenderTarget2D>& IntermediateTexture)
{
	if (!IntermediateTexture)
	{
		IntermediateTexture.Reset(CreateRenderTargetFrom(InTexture, false));
	}

	FTextureResource* SourceResource = InTexture->GetResource();
	FTextureResource* IntermediateResource = IntermediateTexture->GameThread_GetRenderTargetResource();

	// Copy source and destination data into respective render targets
	ENQUEUE_RENDER_COMMAND(CopyTextureData)(
		[SourceResource, IntermediateResource](FRHICommandListImmediate& RHICommandList)
		{
			FRDGBuilder GraphBuilder(RHICommandList);
			FGlobalShaderMap* ShaderMap = GetGlobalShaderMap(GMaxRHIFeatureLevel);

			TRefCountPtr<IPooledRenderTarget> InputRT = CreateRenderTarget(SourceResource->GetTextureRHI(), TEXT("Calibrator.CopyToIntermediate.Input"));
			TRefCountPtr<IPooledRenderTarget> OutputRT = CreateRenderTarget(IntermediateResource->GetTextureRHI(), TEXT("Calibrator.CopyToIntermediate.Output"));

			// Set up RDG resources
			FRDGTextureRef InputTexture = GraphBuilder.RegisterExternalTexture(InputRT);
			FRDGTextureRef OutputTexture = GraphBuilder.RegisterExternalTexture(OutputRT);

			AddDrawTexturePass(
				GraphBuilder,
				ShaderMap,
				InputTexture,
				OutputTexture,
				{});

			GraphBuilder.Execute();
		});
}

UTextureRenderTarget2D* FCalibrator::CreateRenderTargetFrom(TObjectPtr<UTexture> InTexture, bool bClearRenderTarget)
{
	if (!InTexture)
		return nullptr;

	// TODO: Allow targets to specify themselves to force linear gamma or not
	bool bLinearGamma = true;
	if (Cast<UMediaTexture>(InTexture))
	{
		// Fallback to default format for media textures
		bLinearGamma = false;
	}
	
	UTextureRenderTarget2D* OutTexture = NewObject<UTextureRenderTarget2D>(GetTransientPackage());
	check(OutTexture);

	OutTexture->ClearColor = FLinearColor::Black;
	OutTexture->bAutoGenerateMips = false;
	OutTexture->bCanCreateUAV = false;
	OutTexture->InitCustomFormat(InTexture->GetSurfaceWidth(), InTexture->GetSurfaceHeight(), PF_B8G8R8A8, bLinearGamma);
	OutTexture->UpdateResourceImmediate(bClearRenderTarget);

	return OutTexture;
}


FText FCalibrator::GetErrorTextForResult(ECalibrationResult Result)
{
	switch (Result)
	{
	case ECalibrationResult::Success:
		return LOCTEXT("CalibrationResultSuccess", "Success");
	case ECalibrationResult::Error_NoOpenCV:
		return LOCTEXT("CalibrationResultNoOpenCV", "Platform does not support OpenCV, calibration is not possible.");
	case ECalibrationResult::Error_InvalidParams:
		return LOCTEXT("CalibrationResultInvalidParams", "Calibration Asset contains invalid parameters. Ensure checkerboard parameters are set correctly.");
	case ECalibrationResult::Error_MissingSourceOrDestination:
		return LOCTEXT("CalibrationResultMissingSourceOrDestination", "Source or Destination target feed was missing. Ensure a valid target is selected before calibration.");
	case ECalibrationResult::Error_MissingIntrinsics:
		return LOCTEXT("CalibrationResultMissingIntrinsics", "Source or Destination target feed did not supply camera intrinsics. Ensure a valid target is selected before calibration.");
	case ECalibrationResult::Error_ReadTextureFailure:
		return LOCTEXT("CalibrationResultReadTextureFailure", "Internal error: Failed to read data from texture.");
	case ECalibrationResult::Error_IdentifyCheckerboardFailure:
		return LOCTEXT("CalibrationResultIdentifyCheckerboardFailure", "Failed to identify checkerboard in either source feed, destination feed, or both. Please retry.");
	case ECalibrationResult::Error_PointCountMismatch:
		return LOCTEXT("CalibrationResultIdentifyCheckerboardFailure", "Number of points found did not match expected number of points. Please retry.");
	case ECalibrationResult::Error_DrawCheckerboardFailure:
		return LOCTEXT("CalibrationResultDrawCheckerboardFailure", "Internal error: Failed to draw debug checkerboard.");
	case ECalibrationResult::Error_SolvePoseFailure:
		return LOCTEXT("CalibrationResultSolvePoseFailure", "Failed to solve for camera pose. Please retry.");
	}

	checkNoEntry();
	return LOCTEXT("CalibrationResultUnhandledCase", "Unhandled ECalibrationResult in GetErrorTextForResult!");
}


#undef LOCTEXT_NAMESPACE
