//=============================================================================
//
//                  Copyright (c) 2018 QUALCOMM Technologies Inc.
//                              All Rights Reserved.
//
//==============================================================================

#include "Classes/SnapdragonVRControllerFunctionLibrary.h"
#include "SnapdragonVRControllerPrivate.h"
#include "SnapdragonVRController.h"

USnapdragonVRControllerFunctionLibrary::USnapdragonVRControllerFunctionLibrary(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

#if SNAPDRAGONVRCONTROLLER_SUPPORTED_PLATFORMS
FSnapdragonVRController * GetSnapdragonVRController()
{
	TArray<IMotionController*> MotionControllers = IModularFeatures::Get().GetModularFeatureImplementations<IMotionController>(IMotionController::GetModularFeatureName());
	for (auto MotionController : MotionControllers)
	{
		if (MotionController != nullptr && MotionController->GetMotionControllerDeviceTypeName() == FSnapdragonVRController::DeviceTypeName)
		{
			return static_cast<FSnapdragonVRController*>(MotionController);
		}
	}

	return nullptr;
}
#endif // SNAPDRAGONVRCONTROLLER_SUPPORTED_PLATFORMS

bool USnapdragonVRControllerFunctionLibrary::GetControllerOrientationAndPosition(int32 ControllerIndex, EControllerHand Hand, FRotator& OutOrientation, FVector& OutPosition)
{
	bool Result = false;

#if SNAPDRAGONVRCONTROLLER_SUPPORTED_PLATFORMS
	FSnapdragonVRController* SnapdragonVRController = GetSnapdragonVRController();
	if (SnapdragonVRController)
	{
		Result = SnapdragonVRController->GetControllerOrientationAndPosition(ControllerIndex, Hand, OutOrientation, OutPosition, SnapdragonVRController->GetWorldToMetersScale());
	}
#endif // SNAPDRAGONVRCONTROLLER_SUPPORTED_PLATFORMS

	return Result;
}

bool USnapdragonVRControllerFunctionLibrary::GetControllerBatteryLevel(int32 ControllerIndex, EControllerHand Hand, int32 & OutBatteryLevel)
{
	bool Result = false;

#if SNAPDRAGONVRCONTROLLER_SUPPORTED_PLATFORMS
	FSnapdragonVRController* SnapdragonVRController = GetSnapdragonVRController();
	if (SnapdragonVRController)
	{
		OutBatteryLevel = SnapdragonVRController->GetBatteryLevel(ControllerIndex, Hand);

		if (OutBatteryLevel > 0)
		{
			Result = true;
		}
	}
#endif // SNAPDRAGONVRCONTROLLER_SUPPORTED_PLATFORMS

	return Result;
}

bool USnapdragonVRControllerFunctionLibrary::Recenter(int32 ControllerIndex, EControllerHand Hand)
{
	bool Result = false;

#if SNAPDRAGONVRCONTROLLER_SUPPORTED_PLATFORMS

	FSnapdragonVRController* SnapdragonVRController = GetSnapdragonVRController();
	if (SnapdragonVRController)
	{
		Result = SnapdragonVRController->Recenter(ControllerIndex, Hand);
	}

#endif

	return Result;
}

USnapdragonVRControllerEventManager* USnapdragonVRControllerFunctionLibrary::GetSnapdragonVRControllerEventManager()
{
	return USnapdragonVRControllerEventManager::GetInstance();
}
