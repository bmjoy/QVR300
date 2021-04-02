//=============================================================================
//
//                  Copyright (c) 2018 QUALCOMM Technologies Inc.
//                              All Rights Reserved.
//
//==============================================================================

#pragma once

#include "Kismet/BlueprintFunctionLibrary.h"
#include "IMotionController.h"
#include "SnapdragonVRControllerEventManager.h"
#include "SnapdragonVRControllerFunctionLibrary.generated.h"


/**
* Snapdragon VR Controller Extensions Function Library
*/
UCLASS()
class USnapdragonVRControllerFunctionLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_UCLASS_BODY()

public:

	UFUNCTION(BlueprintPure, Category = "SnapdragonVRController")
		static bool GetControllerOrientationAndPosition(int32 ControllerIndex, EControllerHand Hand, FRotator& OutOrientation, FVector& OutPosition);

	UFUNCTION(BlueprintPure, Category = "SnapdragonVRController")
		static bool GetControllerBatteryLevel(int32 ControllerIndex, EControllerHand Hand, int32 & OutBatteryLevel);

	UFUNCTION(BlueprintCallable, Category = "SnapdragonVRController")
		static bool Recenter(int32 ControllerIndex, EControllerHand Hand);

	UFUNCTION(BlueprintCallable, Category = "SnapdragonVRController", meta = (Keywords = "SnapdragonVRController"))
		static USnapdragonVRControllerEventManager* GetSnapdragonVRControllerEventManager();
};
