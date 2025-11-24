#include "Composure/CompUtilsElementInput.h"

TWeakObjectPtr<UCompositionUtilsCameraInput> UCompositionUtilsCameraInput::TryGetCameraInputPassFromCompositingElement(const TWeakObjectPtr<ACompositingElement>& CompositingElement)
{
	TWeakObjectPtr<UCompositionUtilsCameraInput> OutPtr;

	if (CompositingElement.IsValid())
	{
		UTexture* Unused;
		if (UCompositingElementInput* InputPass = CompositingElement->FindInputPass(UCompositionUtilsCameraInput::StaticClass(), Unused))
		{
			if (UCompositionUtilsCameraInput* CameraInputPass = Cast<UCompositionUtilsCameraInput>(InputPass))
			{
				OutPtr = CameraInputPass;
			}
		}
	}

	return OutPtr;
}
