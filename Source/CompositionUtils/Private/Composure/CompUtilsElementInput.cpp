#include "Composure/CompUtilsElementInput.h"

TWeakObjectPtr<UCompositionUtilsAuxiliaryCameraInput> UCompositionUtilsAuxiliaryCameraInput::TryGetAuxCameraInputPassFromCompositingElement(const TWeakObjectPtr<ACompositingElement>& CompositingElement)
{
	TWeakObjectPtr<UCompositionUtilsAuxiliaryCameraInput> OutPtr;

	if (CompositingElement.IsValid())
	{
		UTexture* Unused;
		if (UCompositingElementInput* InputPass = CompositingElement->FindInputPass(UCompositionUtilsAuxiliaryCameraInput::StaticClass(), Unused))
		{
			if (UCompositionUtilsAuxiliaryCameraInput* AuxiliaryCameraInputPass = Cast<UCompositionUtilsAuxiliaryCameraInput>(InputPass))
			{
				OutPtr = AuxiliaryCameraInputPass;
			}
		}
	}

	return OutPtr;
}
