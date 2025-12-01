#include "Calibrator.h"

#include "MediaTexture.h"
#include "RenderGraphBuilder.h"
#include "RenderGraphUtils.h"


bool FCalibrator::RunCalibration(TObjectPtr<UTexture> Source, TObjectPtr<UTexture> Destination)
{
	if (!Source || !Destination)
	{
		return false;
	}

	CopyToIntermediate(Source, SourceIntermediateRT);
	CopyToIntermediate(Destination, DestinationIntermediateRT);

	return true;
}

TObjectPtr<UTexture> FCalibrator::GetCalibratedSourceDebugView() const
{
	return SourceIntermediateRT.IsValid() ? SourceIntermediateRT.Get() : nullptr;
}

TObjectPtr<UTexture> FCalibrator::GetCalibratedDestinationDebugView() const
{
	return DestinationIntermediateRT.IsValid() ? DestinationIntermediateRT.Get() : nullptr;
}

void FCalibrator::InvalidateTransientResources()
{
	SourceIntermediateRT.Reset();
	DestinationIntermediateRT.Reset();

	SourceDebugView.Reset();
	DestinationDebugView.Reset();
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

	// Fallback format
	EPixelFormat Format = PF_R8G8B8A8;
	FIntPoint Size = FIntPoint::ZeroValue;
	bool bLinearGamma = true;

	if (UTexture2D* Texture2D = Cast<UTexture2D>(InTexture))
	{
		Format = Texture2D->GetPixelFormat();
		Size.X = Texture2D->GetSizeX();
		Size.Y = Texture2D->GetSizeY();
	}
	else if (UTextureRenderTarget2D* TextureRenderTarget2D = Cast<UTextureRenderTarget2D>(InTexture))
	{
		Format = TextureRenderTarget2D->GetFormat();
		Size.X = TextureRenderTarget2D->GetSurfaceWidth();
		Size.Y = TextureRenderTarget2D->GetSurfaceHeight();
	}
	else if (UMediaTexture* MediaTexture = Cast<UMediaTexture>(InTexture))
	{
		// Fallback to default format for media textures
		Size.X = MediaTexture->GetWidth();
		Size.Y = MediaTexture->GetHeight();
		bLinearGamma = false;
	}
	else
	{
		check(false && "Unhandled texture type!");
	}
	
	UTextureRenderTarget2D* OutTexture = NewObject<UTextureRenderTarget2D>(GetTransientPackage());
	check(OutTexture);

	OutTexture->ClearColor = FLinearColor::Black;
	OutTexture->bAutoGenerateMips = false;
	OutTexture->bCanCreateUAV = false;
	OutTexture->InitCustomFormat(Size.X, Size.Y, Format, bLinearGamma);
	OutTexture->UpdateResourceImmediate(bClearRenderTarget);

	return OutTexture;
}
