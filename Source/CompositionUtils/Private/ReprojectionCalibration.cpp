#include "ReprojectionCalibration.h"

#include "MediaTexture.h"


UReprojectionCalibration::UReprojectionCalibration()
	: ExtrinsicTransform(FTransform::Identity)
{
}


TObjectPtr<UTexture> UReprojectionCalibrationMediaTarget::GetTexture()
{
	return MediaTexture;
}
