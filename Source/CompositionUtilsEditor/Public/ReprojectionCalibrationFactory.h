#pragma once

#include "CoreMinimal.h"
#include "Factories/Factory.h"

#include "ReprojectionCalibrationFactory.generated.h"

UCLASS()
class COMPOSITIONUTILSEDITOR_API UReprojectionCalibrationFactory : public UFactory
{
	GENERATED_BODY()

public:
	UReprojectionCalibrationFactory();
	virtual UObject* FactoryCreateNew(UClass* InClass, UObject* InParent, FName InName, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn) override;
};
