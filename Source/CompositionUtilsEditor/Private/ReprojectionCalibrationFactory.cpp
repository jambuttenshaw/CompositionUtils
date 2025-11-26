#include "ReprojectionCalibrationFactory.h"

#include "ReprojectionCalibration.h"

UReprojectionCalibrationFactory::UReprojectionCalibrationFactory()
{
	SupportedClass = UReprojectionCalibration::StaticClass();
	bCreateNew = true;
}

UObject* UReprojectionCalibrationFactory::FactoryCreateNew(UClass* InClass, UObject* InParent, FName InName, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn)
{
	return NewObject<UReprojectionCalibration>(InParent, InClass, InName, Flags, Context);
}
