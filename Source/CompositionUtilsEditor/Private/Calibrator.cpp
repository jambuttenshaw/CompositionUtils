#include "Calibrator.h"

#include "CompositionUtilsEditor.h"
#include "MediaTexture.h"
#include "RenderGraphBuilder.h"
#include "RenderGraphUtils.h"

#include "ReprojectionCalibration.h"

#include "OpenCVHelper.h"

#define LOCTEXT_NAMESPACE "FCompositionUtilsEditorModule"


void FCalibrator::ResetCalibrationState(TObjectPtr<UReprojectionCalibration> InAsset)
{
	Asset = InAsset;

	SourceCorners.Empty();
	DestinationCorners.Empty();

	for (auto& Resources : TransientResources)
	{
		Resources.bValidDebugView = false;
	}

	// TODO: Notes on coordinate space
	const auto& Dims = Asset->CheckerboardDimensions;
	ObjectPoints.Empty(Dims.X * Dims.Y);
	for (int32 Y = 0; Y < Dims.Y; Y++)
	{
		for (int32 X = 0; X < Dims.X; X++)
		{
			ObjectPoints.Add(FVector{ X * Asset->CheckerboardSize, Y * Asset->CheckerboardSize, 0.0 });
		}
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
	TObjectPtr<UTexture> Source, 
	TObjectPtr<UTexture> Destination,
	FTransform& OutSourceToDestination)
{
	// Owner of Calibrator should ALWAYS make sure ResetCalibrationState has been called before RunCalibration
	check(Asset);

	if (!(Asset->CheckerboardDimensions.X > 0 && Asset->CheckerboardDimensions.Y > 0 && Asset->CheckerboardSize > 0.0f))	
	{
		return ECalibrationResult::Error_InvalidParams;
	}

	for (auto& Resources : TransientResources)
	{
		Resources.bValidDebugView = false;
	}

	// Calibration relies on OpenCV to run
#if WITH_OPENCV
	if (!Source || !Destination)
	{
		return ECalibrationResult::Error_MissingSourceOrDestination;
	}

	// First, find checkerboard corners for the current pair of images
	ECalibrationResult Result = FindCheckerboardCorners(Asset, Source, TransientResources[Resources_Source], SourceCorners);
	if (Result != ECalibrationResult::Success)
	{
		return Result;
	}

	Result = FindCheckerboardCorners(Asset, Destination, TransientResources[Resources_Destination], DestinationCorners);
	if (Result != ECalibrationResult::Success)
	{
		// To make it less confusing, either show both successful debug images or neither
		TransientResources[Resources_Source].bValidDebugView = false;
		return Result;
	}

	if (SourceCorners.Num() != ObjectPoints.Num() || DestinationCorners.Num() != ObjectPoints.Num())
	{
		return ECalibrationResult::Error_PointCountMismatch;
	}

	// With checkerboard corners found, attempt to solve for the pose of each camera
	FTransform SourceCameraPose, DestinationCameraPose;

	FVector2D FocalLength{ 531.72, 531.72 };
	FVector2D ImageCentre{ 613.57, 329.28 };
	TArray<float> Distortion{ 0, 0, 0, 0, 0 };

	if (!FOpenCVHelper::SolvePnP(
		ObjectPoints,
		SourceCorners,
		FocalLength, ImageCentre, Distortion,
		SourceCameraPose))
	{
		return ECalibrationResult::Error_SolvePoseFailure;
	}

	if (!FOpenCVHelper::SolvePnP(
		ObjectPoints,
		DestinationCorners,
		FocalLength, ImageCentre, Distortion,
		DestinationCameraPose))
	{
		return ECalibrationResult::Error_SolvePoseFailure;
	}

	// Find the transform to get from Source to Destination
	FQuat SourceRotation = SourceCameraPose.GetRotation();
	FQuat DestinationRotation = DestinationCameraPose.GetRotation();
	FVector SourceTranslation = SourceCameraPose.GetTranslation();
	FVector DestinationTranslation = DestinationCameraPose.GetTranslation();

	OutSourceToDestination = FTransform::Identity;
	OutSourceToDestination.SetRotation(SourceRotation * DestinationRotation.Inverse());
	OutSourceToDestination.SetTranslation(SourceTranslation - DestinationTranslation);

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
	TObjectPtr<UReprojectionCalibration> Asset, 
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

	if (!FOpenCVHelper::IdentifyCheckerboard(ImageData, SourceImageSize, Asset->CheckerboardDimensions, OutCorners))
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

	if (!FOpenCVHelper::DrawCheckerboardCorners(OutCorners, Asset->CheckerboardDimensions, Resources.DebugView.Get()))
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