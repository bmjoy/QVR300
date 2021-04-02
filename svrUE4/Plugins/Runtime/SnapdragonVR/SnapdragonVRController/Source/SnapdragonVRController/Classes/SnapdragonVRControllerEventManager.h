//=============================================================================
//
//                  Copyright (c) 2018 QUALCOMM Technologies Inc.
//                              All Rights Reserved.
//
//==============================================================================

#pragma once

#include "CoreMinimal.h"
#include "Delegates/Delegate.h"
#include "UObject/Object.h"
#include "UObject/WeakObjectPtr.h"
#include "SnapdragonVRControllerEventManager.generated.h"

UENUM(BlueprintType)
enum class ESnapdragonVRControllerState : uint8
{
	Disconnected = 1,
	Connected = 2,
	Connecting = 3,
	Error = 4
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FSnapdragonVRControllerRecenterDelegate, int32, ControllerIndex, EControllerHand, Hand);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FSnapdragonVRControllerStateChangeDelegate, int32, ControllerIndex, EControllerHand, hand, ESnapdragonVRControllerState, NewControllerState);

UCLASS()
class USnapdragonVRControllerEventManager : public UObject
{
	GENERATED_UCLASS_BODY()

public:

	UPROPERTY(BlueprintAssignable)
		FSnapdragonVRControllerRecenterDelegate OnControllerRecenteredDelegate;

	UPROPERTY(BlueprintAssignable)
		FSnapdragonVRControllerStateChangeDelegate OnControllerStateChangedDelegate;

public:
	static USnapdragonVRControllerEventManager* GetInstance();
};