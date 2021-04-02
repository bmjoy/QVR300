//=============================================================================
//
//                  Copyright (c) 2018 QUALCOMM Technologies Inc.
//                              All Rights Reserved.
//
//==============================================================================

#include "SnapdragonVRController.h"
#include "Classes/SnapdragonVRControllerFunctionLibrary.h"
#include "CoreDelegates.h"
#include "IHeadMountedDisplay.h"

//#include "Engine/Engine.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "GameFramework/WorldSettings.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "HAL/UnrealMemory.h"

DEFINE_LOG_CATEGORY_STATIC(LogSnapdragonVRController, Log, All);
#define LOCTEXT_NAMESPACE "SnapdragonVRController"

class FSnapdragonVRControllerPlugin : public ISnapdragonVRControllerPlugin
{
	virtual void StartupModule() override
	{
		IInputDeviceModule::StartupModule();
		FSnapdragonVRController::PreInit();
	}

	virtual TSharedPtr< class IInputDevice > CreateInputDevice(const TSharedRef< FGenericApplicationMessageHandler >& InMessageHandler) override
	{
#if SNAPDRAGONVRCONTROLLER_SUPPORTED_PLATFORMS || (PLATFORM_WINDOWS && WINVER > 0x0502)

		UE_LOG(LogSnapdragonVRController, Log, TEXT("Creating Input Device: SnapdragonVRController"));
			
		return TSharedPtr< class IInputDevice >(new FSnapdragonVRController(InMessageHandler));
#else
		return nullptr;
#endif

	}
};



IMPLEMENT_MODULE(FSnapdragonVRControllerPlugin, SnapdragonVRController)

FName FSnapdragonVRController::DeviceTypeName(TEXT("SnapdragonVRController"));

FSnapdragonVRController::FSnapdragonVRController(const TSharedRef< FGenericApplicationMessageHandler >& InMessageHandler)
	: MessageHandler(InMessageHandler),
	bIsTracking(false)
{
	UE_LOG(LogSnapdragonVRController, Log, TEXT("Snapdragon VR Controller Created"));
	
	// Register motion controller!
	IModularFeatures::Get().RegisterModularFeature(GetModularFeatureName(), this);

	StartTracking();

#if SNAPDRAGONVRCONTROLLER_SUPPORTED_PLATFORMS

	// Setup input mappings
	//Touch
	Touches[(int32)EControllerHand::Left][ESnapdragonVRControllerTouch::One] = SnapdragonVRControllerKeyNames::SnapdragonVRController_Left_Touch1;
	Touches[(int32)EControllerHand::Left][ESnapdragonVRControllerTouch::Two] = SnapdragonVRControllerKeyNames::SnapdragonVRController_Left_Touch2;
	Touches[(int32)EControllerHand::Left][ESnapdragonVRControllerTouch::Three] = SnapdragonVRControllerKeyNames::SnapdragonVRController_Left_Touch3;
	Touches[(int32)EControllerHand::Left][ESnapdragonVRControllerTouch::Four] = SnapdragonVRControllerKeyNames::SnapdragonVRController_Left_Touch4;
	Touches[(int32)EControllerHand::Left][ESnapdragonVRControllerTouch::PrimaryThumbstick] = SnapdragonVRControllerKeyNames::SnapdragonVRController_Left_TouchPrimaryThumbstick;
	Touches[(int32)EControllerHand::Left][ESnapdragonVRControllerTouch::SecondaryThumbstick] = SnapdragonVRControllerKeyNames::SnapdragonVRController_Left_TouchSecondaryThumbstick;

	Touches[(int32)EControllerHand::Right][ESnapdragonVRControllerTouch::One] = SnapdragonVRControllerKeyNames::SnapdragonVRController_Right_Touch1;
	Touches[(int32)EControllerHand::Right][ESnapdragonVRControllerTouch::Two] = SnapdragonVRControllerKeyNames::SnapdragonVRController_Right_Touch2;
	Touches[(int32)EControllerHand::Right][ESnapdragonVRControllerTouch::Three] = SnapdragonVRControllerKeyNames::SnapdragonVRController_Right_Touch3;
	Touches[(int32)EControllerHand::Right][ESnapdragonVRControllerTouch::Four] = SnapdragonVRControllerKeyNames::SnapdragonVRController_Right_Touch4;
	Touches[(int32)EControllerHand::Right][ESnapdragonVRControllerTouch::PrimaryThumbstick] = SnapdragonVRControllerKeyNames::SnapdragonVRController_Right_TouchPrimaryThumbstick;
	Touches[(int32)EControllerHand::Right][ESnapdragonVRControllerTouch::SecondaryThumbstick] = SnapdragonVRControllerKeyNames::SnapdragonVRController_Right_TouchSecondaryThumbstick;

	//Buttons
	Buttons[(int32)EControllerHand::Left][ESnapdragonVRControllerButton::One] = SnapdragonVRControllerKeyNames::SnapdragonVRController_Left_Facebutton1;
	Buttons[(int32)EControllerHand::Left][ESnapdragonVRControllerButton::Two] = SnapdragonVRControllerKeyNames::SnapdragonVRController_Left_Facebutton2;
	Buttons[(int32)EControllerHand::Left][ESnapdragonVRControllerButton::Three] = SnapdragonVRControllerKeyNames::SnapdragonVRController_Left_Facebutton3;
	Buttons[(int32)EControllerHand::Left][ESnapdragonVRControllerButton::Four] = SnapdragonVRControllerKeyNames::SnapdragonVRController_Left_Facebutton4;
	Buttons[(int32)EControllerHand::Left][ESnapdragonVRControllerButton::DpadUp] = SnapdragonVRControllerKeyNames::SnapdragonVRController_Left_DPadUp;
	Buttons[(int32)EControllerHand::Left][ESnapdragonVRControllerButton::DpadDown] = SnapdragonVRControllerKeyNames::SnapdragonVRController_Left_DPadDown;
	Buttons[(int32)EControllerHand::Left][ESnapdragonVRControllerButton::DpadLeft] = SnapdragonVRControllerKeyNames::SnapdragonVRController_Left_DPadLeft;
	Buttons[(int32)EControllerHand::Left][ESnapdragonVRControllerButton::DpadRight] = SnapdragonVRControllerKeyNames::SnapdragonVRController_Left_DPadRight;
	Buttons[(int32)EControllerHand::Left][ESnapdragonVRControllerButton::Start] = SnapdragonVRControllerKeyNames::SnapdragonVRController_Left_Start;
	Buttons[(int32)EControllerHand::Left][ESnapdragonVRControllerButton::Back] = SnapdragonVRControllerKeyNames::SnapdragonVRController_Left_Back;
	Buttons[(int32)EControllerHand::Left][ESnapdragonVRControllerButton::PrimaryShoulder] = SnapdragonVRControllerKeyNames::SnapdragonVRController_Left_PrimaryShoulder;
	Buttons[(int32)EControllerHand::Left][ESnapdragonVRControllerButton::PrimaryIndexTrigger] = SnapdragonVRControllerKeyNames::SnapdragonVRController_Left_PrimaryIndexTrigger;
	Buttons[(int32)EControllerHand::Left][ESnapdragonVRControllerButton::PrimaryHandTrigger] = SnapdragonVRControllerKeyNames::SnapdragonVRController_Left_PrimaryHandTrigger;
	Buttons[(int32)EControllerHand::Left][ESnapdragonVRControllerButton::PrimaryThumbstick] = SnapdragonVRControllerKeyNames::SnapdragonVRController_Left_PrimaryThumbstick;
	Buttons[(int32)EControllerHand::Left][ESnapdragonVRControllerButton::PrimaryThumbstickUp] = SnapdragonVRControllerKeyNames::SnapdragonVRController_Left_PrimaryThumbstickUp;
	Buttons[(int32)EControllerHand::Left][ESnapdragonVRControllerButton::PrimaryThumbstickDown] = SnapdragonVRControllerKeyNames::SnapdragonVRController_Left_PrimaryThumbstickDown;
	Buttons[(int32)EControllerHand::Left][ESnapdragonVRControllerButton::PrimaryThumbstickLeft] = SnapdragonVRControllerKeyNames::SnapdragonVRController_Left_PrimaryThumbstickLeft;
	Buttons[(int32)EControllerHand::Left][ESnapdragonVRControllerButton::PrimaryThumbstickRight] = SnapdragonVRControllerKeyNames::SnapdragonVRController_Left_PrimaryThumbstickRight;
	Buttons[(int32)EControllerHand::Left][ESnapdragonVRControllerButton::SecondaryShoulder] = SnapdragonVRControllerKeyNames::SnapdragonVRController_Left_SecondaryShoulder;
	Buttons[(int32)EControllerHand::Left][ESnapdragonVRControllerButton::SecondaryIndexTrigger] = SnapdragonVRControllerKeyNames::SnapdragonVRController_Left_SecondaryIndexTrigger;
	Buttons[(int32)EControllerHand::Left][ESnapdragonVRControllerButton::SecondaryHandTrigger] = SnapdragonVRControllerKeyNames::SnapdragonVRController_Left_SecondaryHandTrigger;
	Buttons[(int32)EControllerHand::Left][ESnapdragonVRControllerButton::SecondaryThumbstick] = SnapdragonVRControllerKeyNames::SnapdragonVRController_Left_SecondaryThumbstick;
	Buttons[(int32)EControllerHand::Left][ESnapdragonVRControllerButton::SecondaryThumbstickUp] = SnapdragonVRControllerKeyNames::SnapdragonVRController_Left_SecondaryThumbstickUp;
	Buttons[(int32)EControllerHand::Left][ESnapdragonVRControllerButton::SecondaryThumbstickDown] = SnapdragonVRControllerKeyNames::SnapdragonVRController_Left_SecondaryThumbstickDown;
	Buttons[(int32)EControllerHand::Left][ESnapdragonVRControllerButton::SecondaryThumbstickLeft] = SnapdragonVRControllerKeyNames::SnapdragonVRController_Left_SecondaryThumbstickLeft;
	Buttons[(int32)EControllerHand::Left][ESnapdragonVRControllerButton::SecondaryThumbstickRight] = SnapdragonVRControllerKeyNames::SnapdragonVRController_Left_SecondaryThumbstickRight;
	Buttons[(int32)EControllerHand::Left][ESnapdragonVRControllerButton::Up] = SnapdragonVRControllerKeyNames::SnapdragonVRController_Left_ButtonUp;
	Buttons[(int32)EControllerHand::Left][ESnapdragonVRControllerButton::Down] = SnapdragonVRControllerKeyNames::SnapdragonVRController_Left_ButtonDown;
	Buttons[(int32)EControllerHand::Left][ESnapdragonVRControllerButton::Left] = SnapdragonVRControllerKeyNames::SnapdragonVRController_Left_ButtonLeft;
	Buttons[(int32)EControllerHand::Left][ESnapdragonVRControllerButton::Right] = SnapdragonVRControllerKeyNames::SnapdragonVRController_Left_ButtonRight;

	Buttons[(int32)EControllerHand::Right][ESnapdragonVRControllerButton::One] = SnapdragonVRControllerKeyNames::SnapdragonVRController_Right_Facebutton1;
	Buttons[(int32)EControllerHand::Right][ESnapdragonVRControllerButton::Two] = SnapdragonVRControllerKeyNames::SnapdragonVRController_Right_Facebutton2;
	Buttons[(int32)EControllerHand::Right][ESnapdragonVRControllerButton::Three] = SnapdragonVRControllerKeyNames::SnapdragonVRController_Right_Facebutton3;
	Buttons[(int32)EControllerHand::Right][ESnapdragonVRControllerButton::Four] = SnapdragonVRControllerKeyNames::SnapdragonVRController_Right_Facebutton4;
	Buttons[(int32)EControllerHand::Right][ESnapdragonVRControllerButton::DpadUp] = SnapdragonVRControllerKeyNames::SnapdragonVRController_Right_DPadUp;
	Buttons[(int32)EControllerHand::Right][ESnapdragonVRControllerButton::DpadDown] = SnapdragonVRControllerKeyNames::SnapdragonVRController_Right_DPadDown;
	Buttons[(int32)EControllerHand::Right][ESnapdragonVRControllerButton::DpadLeft] = SnapdragonVRControllerKeyNames::SnapdragonVRController_Right_DPadLeft;
	Buttons[(int32)EControllerHand::Right][ESnapdragonVRControllerButton::DpadRight] = SnapdragonVRControllerKeyNames::SnapdragonVRController_Right_DPadRight;
	Buttons[(int32)EControllerHand::Right][ESnapdragonVRControllerButton::Start] = SnapdragonVRControllerKeyNames::SnapdragonVRController_Right_Start;
	Buttons[(int32)EControllerHand::Right][ESnapdragonVRControllerButton::Back] = SnapdragonVRControllerKeyNames::SnapdragonVRController_Right_Back;
	Buttons[(int32)EControllerHand::Right][ESnapdragonVRControllerButton::PrimaryShoulder] = SnapdragonVRControllerKeyNames::SnapdragonVRController_Right_PrimaryShoulder;
	Buttons[(int32)EControllerHand::Right][ESnapdragonVRControllerButton::PrimaryIndexTrigger] = SnapdragonVRControllerKeyNames::SnapdragonVRController_Right_PrimaryIndexTrigger;
	Buttons[(int32)EControllerHand::Right][ESnapdragonVRControllerButton::PrimaryHandTrigger] = SnapdragonVRControllerKeyNames::SnapdragonVRController_Right_PrimaryHandTrigger;
	Buttons[(int32)EControllerHand::Right][ESnapdragonVRControllerButton::PrimaryThumbstick] = SnapdragonVRControllerKeyNames::SnapdragonVRController_Right_PrimaryThumbstick;
	Buttons[(int32)EControllerHand::Right][ESnapdragonVRControllerButton::PrimaryThumbstickUp] = SnapdragonVRControllerKeyNames::SnapdragonVRController_Right_PrimaryThumbstickUp;
	Buttons[(int32)EControllerHand::Right][ESnapdragonVRControllerButton::PrimaryThumbstickDown] = SnapdragonVRControllerKeyNames::SnapdragonVRController_Right_PrimaryThumbstickDown;
	Buttons[(int32)EControllerHand::Right][ESnapdragonVRControllerButton::PrimaryThumbstickLeft] = SnapdragonVRControllerKeyNames::SnapdragonVRController_Right_PrimaryThumbstickLeft;
	Buttons[(int32)EControllerHand::Right][ESnapdragonVRControllerButton::PrimaryThumbstickRight] = SnapdragonVRControllerKeyNames::SnapdragonVRController_Right_PrimaryThumbstickRight;
	Buttons[(int32)EControllerHand::Right][ESnapdragonVRControllerButton::SecondaryShoulder] = SnapdragonVRControllerKeyNames::SnapdragonVRController_Right_SecondaryShoulder;
	Buttons[(int32)EControllerHand::Right][ESnapdragonVRControllerButton::SecondaryIndexTrigger] = SnapdragonVRControllerKeyNames::SnapdragonVRController_Right_SecondaryIndexTrigger;
	Buttons[(int32)EControllerHand::Right][ESnapdragonVRControllerButton::SecondaryHandTrigger] = SnapdragonVRControllerKeyNames::SnapdragonVRController_Right_SecondaryHandTrigger;
	Buttons[(int32)EControllerHand::Right][ESnapdragonVRControllerButton::SecondaryThumbstick] = SnapdragonVRControllerKeyNames::SnapdragonVRController_Right_SecondaryThumbstick;
	Buttons[(int32)EControllerHand::Right][ESnapdragonVRControllerButton::SecondaryThumbstickUp] = SnapdragonVRControllerKeyNames::SnapdragonVRController_Right_SecondaryThumbstickUp;
	Buttons[(int32)EControllerHand::Right][ESnapdragonVRControllerButton::SecondaryThumbstickDown] = SnapdragonVRControllerKeyNames::SnapdragonVRController_Right_SecondaryThumbstickDown;
	Buttons[(int32)EControllerHand::Right][ESnapdragonVRControllerButton::SecondaryThumbstickLeft] = SnapdragonVRControllerKeyNames::SnapdragonVRController_Right_SecondaryThumbstickLeft;
	Buttons[(int32)EControllerHand::Right][ESnapdragonVRControllerButton::SecondaryThumbstickRight] = SnapdragonVRControllerKeyNames::SnapdragonVRController_Right_SecondaryThumbstickRight;
	Buttons[(int32)EControllerHand::Right][ESnapdragonVRControllerButton::Up] = SnapdragonVRControllerKeyNames::SnapdragonVRController_Right_ButtonUp;
	Buttons[(int32)EControllerHand::Right][ESnapdragonVRControllerButton::Down] = SnapdragonVRControllerKeyNames::SnapdragonVRController_Right_ButtonDown;
	Buttons[(int32)EControllerHand::Right][ESnapdragonVRControllerButton::Left] = SnapdragonVRControllerKeyNames::SnapdragonVRController_Right_ButtonLeft;
	Buttons[(int32)EControllerHand::Right][ESnapdragonVRControllerButton::Right] = SnapdragonVRControllerKeyNames::SnapdragonVRController_Right_ButtonRight;

#endif

	// Register callbacks for pause and resume
	FCoreDelegates::ApplicationWillEnterBackgroundDelegate.AddRaw(this, &FSnapdragonVRController::ApplicationPauseDelegate);
	FCoreDelegates::ApplicationHasEnteredForegroundDelegate.AddRaw(this, &FSnapdragonVRController::ApplicationResumeDelegate);
}

FSnapdragonVRController::~FSnapdragonVRController()
{
	IModularFeatures::Get().UnregisterModularFeature(GetModularFeatureName(), this);

	StopTracking();
}


void FSnapdragonVRController::PreInit()
{
	//Add custom keys

	// Touch
	EKeys::AddKey(FKeyDetails(SnapdragonVRControllerKeys::SnapdragonVRController_Left_Touch1, LOCTEXT("SnapdragonVRController_Left_Touch1", "SnapdragonVR Controller (L) Touch1"), FKeyDetails::GamepadKey));
	EKeys::AddKey(FKeyDetails(SnapdragonVRControllerKeys::SnapdragonVRController_Left_Touch2, LOCTEXT("SnapdragonVRController_Left_Touch2", "SnapdragonVR Controller (L) Touch2"), FKeyDetails::GamepadKey));
	EKeys::AddKey(FKeyDetails(SnapdragonVRControllerKeys::SnapdragonVRController_Left_Touch3, LOCTEXT("SnapdragonVRController_Left_Touch3", "SnapdragonVR Controller (L) Touch3"), FKeyDetails::GamepadKey));
	EKeys::AddKey(FKeyDetails(SnapdragonVRControllerKeys::SnapdragonVRController_Left_Touch4, LOCTEXT("SnapdragonVRController_Left_Touch4", "SnapdragonVR Controller (L) Touch4"), FKeyDetails::GamepadKey));
	EKeys::AddKey(FKeyDetails(SnapdragonVRControllerKeys::SnapdragonVRController_Left_TouchPrimaryThumbstick, LOCTEXT("SnapdragonVRController_Left_TouchPrimaryThumbstick", "SnapdragonVR Controller (L) Touch Primary Thumbstick"), FKeyDetails::GamepadKey));
	EKeys::AddKey(FKeyDetails(SnapdragonVRControllerKeys::SnapdragonVRController_Left_TouchSecondaryThumbstick, LOCTEXT("SnapdragonVRController_Left_TouchSecondaryThumbstick", "SnapdragonVR Controller (L) Touch Secondary Thumbstick"), FKeyDetails::GamepadKey));
	EKeys::AddKey(FKeyDetails(SnapdragonVRControllerKeys::SnapdragonVRController_Right_Touch1, LOCTEXT("SnapdragonVRController_Right_Touch1", "SnapdragonVR Controller (R) Touch1"), FKeyDetails::GamepadKey));
	EKeys::AddKey(FKeyDetails(SnapdragonVRControllerKeys::SnapdragonVRController_Right_Touch2, LOCTEXT("SnapdragonVRController_Right_Touch2", "SnapdragonVR Controller (R) Touch2"), FKeyDetails::GamepadKey));
	EKeys::AddKey(FKeyDetails(SnapdragonVRControllerKeys::SnapdragonVRController_Right_Touch3, LOCTEXT("SnapdragonVRController_Right_Touch3", "SnapdragonVR Controller (R) Touch3"), FKeyDetails::GamepadKey));
	EKeys::AddKey(FKeyDetails(SnapdragonVRControllerKeys::SnapdragonVRController_Right_Touch4, LOCTEXT("SnapdragonVRController_Right_Touch4", "SnapdragonVR Controller (R) Touch4"), FKeyDetails::GamepadKey));
	EKeys::AddKey(FKeyDetails(SnapdragonVRControllerKeys::SnapdragonVRController_Right_TouchPrimaryThumbstick, LOCTEXT("SnapdragonVRController_Right_TouchPrimaryThumbstick", "SnapdragonVR Controller (R) Touch Primary Thumbstick"), FKeyDetails::GamepadKey));
	EKeys::AddKey(FKeyDetails(SnapdragonVRControllerKeys::SnapdragonVRController_Right_TouchSecondaryThumbstick, LOCTEXT("SnapdragonVRController_Right_TouchSecondaryThumbstick", "SnapdragonVR Controller (R) Touch Secondary Thumbstick"), FKeyDetails::GamepadKey));

	// Axis 1D
	EKeys::AddKey(FKeyDetails(SnapdragonVRControllerKeys::SnapdragonVRController_Left_Axis1DPrimaryIndexTrigger, LOCTEXT("SnapdragonVRController_Left_Axis1DPrimaryIndexTrigger", "SnapdragonVR Controller (L) Primary Index Analog Trigger"), FKeyDetails::GamepadKey | FKeyDetails::FloatAxis));
	EKeys::AddKey(FKeyDetails(SnapdragonVRControllerKeys::SnapdragonVRController_Left_Axis1DSecondaryIndexTrigger, LOCTEXT("SnapdragonVRController_Left_Axis1DSecondaryIndexTrigger", "SnapdragonVR Controller (L) Secondary Index Analog Trigger"), FKeyDetails::GamepadKey | FKeyDetails::FloatAxis));
	EKeys::AddKey(FKeyDetails(SnapdragonVRControllerKeys::SnapdragonVRController_Left_Axis1DPrimaryHandTrigger, LOCTEXT("SnapdragonVRController_Left_Axis1DPrimaryHandTrigger", "SnapdragonVR Controller (L) Primary Hand Analog Trigger"), FKeyDetails::GamepadKey | FKeyDetails::FloatAxis));
	EKeys::AddKey(FKeyDetails(SnapdragonVRControllerKeys::SnapdragonVRController_Left_Axis1DSecondaryHandTrigger, LOCTEXT("SnapdragonVRController_Left_Axis1DSecondaryHandTrigger", "SnapdragonVR Controller (L) Secondary Hand Analog Trigger"), FKeyDetails::GamepadKey | FKeyDetails::FloatAxis));
	EKeys::AddKey(FKeyDetails(SnapdragonVRControllerKeys::SnapdragonVRController_Right_Axis1DPrimaryIndexTrigger, LOCTEXT("SnapdragonVRController_Right_Axis1DPrimaryIndexTrigger", "SnapdragonVR Controller (R) Primary Index Analog Trigger"), FKeyDetails::GamepadKey | FKeyDetails::FloatAxis));
	EKeys::AddKey(FKeyDetails(SnapdragonVRControllerKeys::SnapdragonVRController_Right_Axis1DSecondaryIndexTrigger, LOCTEXT("SnapdragonVRController_Right_Axis1DSecondaryIndexTrigger", "SnapdragonVR Controller (R) Secondary Index Analog Trigger"), FKeyDetails::GamepadKey | FKeyDetails::FloatAxis));
	EKeys::AddKey(FKeyDetails(SnapdragonVRControllerKeys::SnapdragonVRController_Right_Axis1DPrimaryHandTrigger, LOCTEXT("SnapdragonVRController_Right_Axis1DPrimaryHandTrigger", "SnapdragonVR Controller (R) Primary Hand Analog Trigger"), FKeyDetails::GamepadKey | FKeyDetails::FloatAxis));
	EKeys::AddKey(FKeyDetails(SnapdragonVRControllerKeys::SnapdragonVRController_Right_Axis1DSecondaryHandTrigger, LOCTEXT("SnapdragonVRController_Right_Axis1DSecondaryHandTrigger", "SnapdragonVR Controller (R) Secondary Hand Analog Trigger"), FKeyDetails::GamepadKey | FKeyDetails::FloatAxis));

	//Axis 2D
	EKeys::AddKey(FKeyDetails(SnapdragonVRControllerKeys::SnapdragonVRController_Left_Axis2DPrimaryThumbstick_X, LOCTEXT("SnapdragonVRController_Left_Axis2DPrimaryThumbstick_X", "SnapdragonVR Controller (L) Primary Thumbstick X"), FKeyDetails::GamepadKey | FKeyDetails::FloatAxis));
	EKeys::AddKey(FKeyDetails(SnapdragonVRControllerKeys::SnapdragonVRController_Left_Axis2DPrimaryThumbstick_Y, LOCTEXT("SnapdragonVRController_Left_Axis2DPrimaryThumbstick_Y", "SnapdragonVR Controller (L) Primary Thumbstick Y"), FKeyDetails::GamepadKey | FKeyDetails::FloatAxis));
	EKeys::AddKey(FKeyDetails(SnapdragonVRControllerKeys::SnapdragonVRController_Left_Axis2DSecondaryThumbstick_X, LOCTEXT("SnapdragonVRController_Left_Axis2DSecondaryThumbstick_X", "SnapdragonVR Controller (L) Secondary Thumbstick X"), FKeyDetails::GamepadKey | FKeyDetails::FloatAxis));
	EKeys::AddKey(FKeyDetails(SnapdragonVRControllerKeys::SnapdragonVRController_Left_Axis2DSecondaryThumbstick_Y, LOCTEXT("SnapdragonVRController_Left_Axis2DSecondaryThumbstick_Y", "SnapdragonVR Controller (L) Secondary Thumbstick Y"), FKeyDetails::GamepadKey | FKeyDetails::FloatAxis));
	EKeys::AddKey(FKeyDetails(SnapdragonVRControllerKeys::SnapdragonVRController_Right_Axis2DPrimaryThumbstick_X, LOCTEXT("SnapdragonVRController_Right_Axis2DPrimaryThumbstick_X", "SnapdragonVR Controller (R) Primary Thumbstick X"), FKeyDetails::GamepadKey | FKeyDetails::FloatAxis));
	EKeys::AddKey(FKeyDetails(SnapdragonVRControllerKeys::SnapdragonVRController_Right_Axis2DPrimaryThumbstick_Y, LOCTEXT("SnapdragonVRController_Right_Axis2DPrimaryThumbstick_Y", "SnapdragonVR Controller (R) Primary Thumbstick Y"), FKeyDetails::GamepadKey | FKeyDetails::FloatAxis));
	EKeys::AddKey(FKeyDetails(SnapdragonVRControllerKeys::SnapdragonVRController_Right_Axis2DSecondaryThumbstick_X, LOCTEXT("SnapdragonVRController_Right_Axis2DSecondaryThumbstick_X", "SnapdragonVR Controller (R) Secondary Thumbstick X"), FKeyDetails::GamepadKey | FKeyDetails::FloatAxis));
	EKeys::AddKey(FKeyDetails(SnapdragonVRControllerKeys::SnapdragonVRController_Right_Axis2DSecondaryThumbstick_Y, LOCTEXT("SnapdragonVRController_Right_Axis2DSecondaryThumbstick_Y", "SnapdragonVR Controller (R) Secondary Thumbstick Y"), FKeyDetails::GamepadKey | FKeyDetails::FloatAxis));


	//Buttons
	EKeys::AddKey(FKeyDetails(SnapdragonVRControllerKeys::SnapdragonVRController_Left_Facebutton1, LOCTEXT("SnapdragonVRController_Left_Facebutton1", "SnapdragonVR Controller (L) Face Button 1"), FKeyDetails::GamepadKey));
	EKeys::AddKey(FKeyDetails(SnapdragonVRControllerKeys::SnapdragonVRController_Left_Facebutton2, LOCTEXT("SnapdragonVRController_Left_Facebutton2", "SnapdragonVR Controller (L) Face Button 2"), FKeyDetails::GamepadKey));
	EKeys::AddKey(FKeyDetails(SnapdragonVRControllerKeys::SnapdragonVRController_Left_Facebutton3, LOCTEXT("SnapdragonVRController_Left_Facebutton3", "SnapdragonVR Controller (L) Face Button 3"), FKeyDetails::GamepadKey));
	EKeys::AddKey(FKeyDetails(SnapdragonVRControllerKeys::SnapdragonVRController_Left_Facebutton4, LOCTEXT("SnapdragonVRController_Left_Facebutton4", "SnapdragonVR Controller (L) Face Button 4"), FKeyDetails::GamepadKey));
	EKeys::AddKey(FKeyDetails(SnapdragonVRControllerKeys::SnapdragonVRController_Left_DPadUp, LOCTEXT("SnapdragonVRController_Left_DPadUp", "SnapdragonVR Controller (L) D-Pad Up"), FKeyDetails::GamepadKey));
	EKeys::AddKey(FKeyDetails(SnapdragonVRControllerKeys::SnapdragonVRController_Left_DPadDown, LOCTEXT("SnapdragonVRController_Left_DPadDown", "SnapdragonVR Controller (L) D-Pad Down"), FKeyDetails::GamepadKey));
	EKeys::AddKey(FKeyDetails(SnapdragonVRControllerKeys::SnapdragonVRController_Left_DPadLeft, LOCTEXT("SnapdragonVRController_Left_DPadLeft", "SnapdragonVR Controller (L) D-Pad Left"), FKeyDetails::GamepadKey));
	EKeys::AddKey(FKeyDetails(SnapdragonVRControllerKeys::SnapdragonVRController_Left_DPadRight, LOCTEXT("SnapdragonVRController_Left_DPadRight", "SnapdragonVR Controller (L) D-Pad Right"), FKeyDetails::GamepadKey));
	EKeys::AddKey(FKeyDetails(SnapdragonVRControllerKeys::SnapdragonVRController_Left_Start, LOCTEXT("SnapdragonVRController_Left_Start", "SnapdragonVR Controller (L) Start"), FKeyDetails::GamepadKey));
	EKeys::AddKey(FKeyDetails(SnapdragonVRControllerKeys::SnapdragonVRController_Left_Back, LOCTEXT("SnapdragonVRController_Left_Back", "SnapdragonVR Controller (L) Back"), FKeyDetails::GamepadKey));
	EKeys::AddKey(FKeyDetails(SnapdragonVRControllerKeys::SnapdragonVRController_Left_PrimaryShoulder, LOCTEXT("SnapdragonVRController_Left_PrimaryShoulder", "SnapdragonVR Controller (L) Primary Shoulder"), FKeyDetails::GamepadKey));
	EKeys::AddKey(FKeyDetails(SnapdragonVRControllerKeys::SnapdragonVRController_Left_PrimaryIndexTrigger, LOCTEXT("SnapdragonVRController_Left_PrimaryIndexTrigger", "SnapdragonVR Controller (L) Primary Index Trigger"), FKeyDetails::GamepadKey));
	EKeys::AddKey(FKeyDetails(SnapdragonVRControllerKeys::SnapdragonVRController_Left_PrimaryHandTrigger, LOCTEXT("SnapdragonVRController_Left_PrimaryHandTrigger", "SnapdragonVR Controller (L) Primary Hand Trigger"), FKeyDetails::GamepadKey));
	EKeys::AddKey(FKeyDetails(SnapdragonVRControllerKeys::SnapdragonVRController_Left_PrimaryThumbstick, LOCTEXT("SnapdragonVRController_Left_PrimaryThumbstick", "SnapdragonVR Controller (L) Primary Thumbstick"), FKeyDetails::GamepadKey));
	EKeys::AddKey(FKeyDetails(SnapdragonVRControllerKeys::SnapdragonVRController_Left_PrimaryThumbstickUp, LOCTEXT("SnapdragonVRController_Left_PrimaryThumbstickUp", "SnapdragonVR Controller (L)Primary Thumbstick Up"), FKeyDetails::GamepadKey));
	EKeys::AddKey(FKeyDetails(SnapdragonVRControllerKeys::SnapdragonVRController_Left_PrimaryThumbstickDown, LOCTEXT("SnapdragonVRController_Left_PrimaryThumbstickDown", "SnapdragonVR Controller (L) Primary Thumbstick Down"), FKeyDetails::GamepadKey));
	EKeys::AddKey(FKeyDetails(SnapdragonVRControllerKeys::SnapdragonVRController_Left_PrimaryThumbstickLeft, LOCTEXT("SnapdragonVRController_Left_PrimaryThumbstickLeft", "SnapdragonVR Controller (L) Primary Thumbstick Left"), FKeyDetails::GamepadKey));
	EKeys::AddKey(FKeyDetails(SnapdragonVRControllerKeys::SnapdragonVRController_Left_PrimaryThumbstickRight, LOCTEXT("SnapdragonVRController_Left_PrimaryThumbstickRight", "SnapdragonVR Controller (L) Primary Thumbstick Right"), FKeyDetails::GamepadKey));
	EKeys::AddKey(FKeyDetails(SnapdragonVRControllerKeys::SnapdragonVRController_Left_SecondaryShoulder, LOCTEXT("SnapdragonVRController_Left_SecondaryShoulder", "SnapdragonVR Controller (L) Secondary Shoulder"), FKeyDetails::GamepadKey));
	EKeys::AddKey(FKeyDetails(SnapdragonVRControllerKeys::SnapdragonVRController_Left_SecondaryIndexTrigger, LOCTEXT("SnapdragonVRController_Left_SecondaryIndexTrigger", "SnapdragonVR Controller (L) Secondary Index Trigger"), FKeyDetails::GamepadKey));
	EKeys::AddKey(FKeyDetails(SnapdragonVRControllerKeys::SnapdragonVRController_Left_SecondaryHandTrigger, LOCTEXT("SnapdragonVRController_Left_SecondaryHandTrigger", "SnapdragonVR Controller (L) Secondary Hand Trigger"), FKeyDetails::GamepadKey));
	EKeys::AddKey(FKeyDetails(SnapdragonVRControllerKeys::SnapdragonVRController_Left_SecondaryThumbstick, LOCTEXT("SnapdragonVRController_Left_SecondaryThumbstick", "SnapdragonVR Controller (L) Secondary Thumbstick"), FKeyDetails::GamepadKey));
	EKeys::AddKey(FKeyDetails(SnapdragonVRControllerKeys::SnapdragonVRController_Left_SecondaryThumbstickUp, LOCTEXT("SnapdragonVRController_Left_SecondaryThumbstickUp", "SnapdragonVR Controller (L)Secondary Thumbstick Up"), FKeyDetails::GamepadKey));
	EKeys::AddKey(FKeyDetails(SnapdragonVRControllerKeys::SnapdragonVRController_Left_SecondaryThumbstickDown, LOCTEXT("SnapdragonVRController_Left_SecondaryThumbstickDown", "SnapdragonVR Controller (L) Secondary Thumbstick Down"), FKeyDetails::GamepadKey));
	EKeys::AddKey(FKeyDetails(SnapdragonVRControllerKeys::SnapdragonVRController_Left_SecondaryThumbstickLeft, LOCTEXT("SnapdragonVRController_Left_SecondaryThumbstickLeft", "SnapdragonVR Controller (L) Secondary Thumbstick Left"), FKeyDetails::GamepadKey));
	EKeys::AddKey(FKeyDetails(SnapdragonVRControllerKeys::SnapdragonVRController_Left_SecondaryThumbstickRight, LOCTEXT("SnapdragonVRController_Left_SecondaryThumbstickRight", "SnapdragonVR Controller (L) Secondary Thumbstick Right"), FKeyDetails::GamepadKey));
	EKeys::AddKey(FKeyDetails(SnapdragonVRControllerKeys::SnapdragonVRController_Left_ButtonUp, LOCTEXT("SnapdragonVRController_Left_ButtonUp", "SnapdragonVR Controller (L) Button Up"), FKeyDetails::GamepadKey));
	EKeys::AddKey(FKeyDetails(SnapdragonVRControllerKeys::SnapdragonVRController_Left_ButtonDown, LOCTEXT("SnapdragonVRController_Left_ButtonDown", "SnapdragonVR Controller (L) Button Down"), FKeyDetails::GamepadKey));
	EKeys::AddKey(FKeyDetails(SnapdragonVRControllerKeys::SnapdragonVRController_Left_ButtonLeft, LOCTEXT("SnapdragonVRController_Left_ButtonLeft", "SnapdragonVR Controller (L) Button Left"), FKeyDetails::GamepadKey));
	EKeys::AddKey(FKeyDetails(SnapdragonVRControllerKeys::SnapdragonVRController_Left_ButtonRight, LOCTEXT("SnapdragonVRController_Left_ButtonRight", "SnapdragonVR Controller (L) Button Right"), FKeyDetails::GamepadKey));
	EKeys::AddKey(FKeyDetails(SnapdragonVRControllerKeys::SnapdragonVRController_Right_Facebutton1, LOCTEXT("SnapdragonVRController_Right_Facebutton1", "SnapdragonVR Controller (R) Face Button 1"), FKeyDetails::GamepadKey));
	EKeys::AddKey(FKeyDetails(SnapdragonVRControllerKeys::SnapdragonVRController_Right_Facebutton2, LOCTEXT("SnapdragonVRController_Right_Facebutton2", "SnapdragonVR Controller (R) Face Button 2"), FKeyDetails::GamepadKey));
	EKeys::AddKey(FKeyDetails(SnapdragonVRControllerKeys::SnapdragonVRController_Right_Facebutton3, LOCTEXT("SnapdragonVRController_Right_Facebutton3", "SnapdragonVR Controller (R) Face Button 3"), FKeyDetails::GamepadKey));
	EKeys::AddKey(FKeyDetails(SnapdragonVRControllerKeys::SnapdragonVRController_Right_Facebutton4, LOCTEXT("SnapdragonVRController_Right_Facebutton4", "SnapdragonVR Controller (R) Face Button 4"), FKeyDetails::GamepadKey));
	EKeys::AddKey(FKeyDetails(SnapdragonVRControllerKeys::SnapdragonVRController_Right_DPadUp, LOCTEXT("SnapdragonVRController_Right_DPadUp", "SnapdragonVR Controller (R) D-Pad Up"), FKeyDetails::GamepadKey));
	EKeys::AddKey(FKeyDetails(SnapdragonVRControllerKeys::SnapdragonVRController_Right_DPadDown, LOCTEXT("SnapdragonVRController_Right_DPadDown", "SnapdragonVR Controller (R) D-Pad Down"), FKeyDetails::GamepadKey));
	EKeys::AddKey(FKeyDetails(SnapdragonVRControllerKeys::SnapdragonVRController_Right_DPadLeft, LOCTEXT("SnapdragonVRController_Right_DPadLeft", "SnapdragonVR Controller (R) D-Pad Left"), FKeyDetails::GamepadKey));
	EKeys::AddKey(FKeyDetails(SnapdragonVRControllerKeys::SnapdragonVRController_Right_DPadRight, LOCTEXT("SnapdragonVRController_Right_DPadRight", "SnapdragonVR Controller (R) D-Pad Right"), FKeyDetails::GamepadKey));
	EKeys::AddKey(FKeyDetails(SnapdragonVRControllerKeys::SnapdragonVRController_Right_Start, LOCTEXT("SnapdragonVRController_Right_Start", "SnapdragonVR Controller (R) Start"), FKeyDetails::GamepadKey));
	EKeys::AddKey(FKeyDetails(SnapdragonVRControllerKeys::SnapdragonVRController_Right_Back, LOCTEXT("SnapdragonVRController_Right_Back", "SnapdragonVR Controller (R) Back"), FKeyDetails::GamepadKey));
	EKeys::AddKey(FKeyDetails(SnapdragonVRControllerKeys::SnapdragonVRController_Right_PrimaryShoulder, LOCTEXT("SnapdragonVRController_Right_PrimaryShoulder", "SnapdragonVR Controller (R) Primary Shoulder"), FKeyDetails::GamepadKey));
	EKeys::AddKey(FKeyDetails(SnapdragonVRControllerKeys::SnapdragonVRController_Right_PrimaryIndexTrigger, LOCTEXT("SnapdragonVRController_Right_PrimaryIndexTrigger", "SnapdragonVR Controller (R) Primary Index Trigger"), FKeyDetails::GamepadKey));
	EKeys::AddKey(FKeyDetails(SnapdragonVRControllerKeys::SnapdragonVRController_Right_PrimaryHandTrigger, LOCTEXT("SnapdragonVRController_Right_PrimaryHandTrigger", "SnapdragonVR Controller (R) Primary Hand Trigger"), FKeyDetails::GamepadKey));
	EKeys::AddKey(FKeyDetails(SnapdragonVRControllerKeys::SnapdragonVRController_Right_PrimaryThumbstick, LOCTEXT("SnapdragonVRController_Right_PrimaryThumbstick", "SnapdragonVR Controller (R) Primary Thumbstick"), FKeyDetails::GamepadKey));
	EKeys::AddKey(FKeyDetails(SnapdragonVRControllerKeys::SnapdragonVRController_Right_PrimaryThumbstickUp, LOCTEXT("SnapdragonVRController_Right_PrimaryThumbstickUp", "SnapdragonVR Controller (R)Primary Thumbstick Up"), FKeyDetails::GamepadKey));
	EKeys::AddKey(FKeyDetails(SnapdragonVRControllerKeys::SnapdragonVRController_Right_PrimaryThumbstickDown, LOCTEXT("SnapdragonVRController_Right_PrimaryThumbstickDown", "SnapdragonVR Controller (R) Primary Thumbstick Down"), FKeyDetails::GamepadKey));
	EKeys::AddKey(FKeyDetails(SnapdragonVRControllerKeys::SnapdragonVRController_Right_PrimaryThumbstickLeft, LOCTEXT("SnapdragonVRController_Right_PrimaryThumbstickLeft", "SnapdragonVR Controller (R) Primary Thumbstick Left"), FKeyDetails::GamepadKey));
	EKeys::AddKey(FKeyDetails(SnapdragonVRControllerKeys::SnapdragonVRController_Right_PrimaryThumbstickRight, LOCTEXT("SnapdragonVRController_Right_PrimaryThumbstickRight", "SnapdragonVR Controller (R) Primary Thumbstick Right"), FKeyDetails::GamepadKey));
	EKeys::AddKey(FKeyDetails(SnapdragonVRControllerKeys::SnapdragonVRController_Right_SecondaryShoulder, LOCTEXT("SnapdragonVRController_Right_SecondaryShoulder", "SnapdragonVR Controller (R) Secondary Shoulder"), FKeyDetails::GamepadKey));
	EKeys::AddKey(FKeyDetails(SnapdragonVRControllerKeys::SnapdragonVRController_Right_SecondaryIndexTrigger, LOCTEXT("SnapdragonVRController_Right_SecondaryIndexTrigger", "SnapdragonVR Controller (R) Secondary Index Trigger"), FKeyDetails::GamepadKey));
	EKeys::AddKey(FKeyDetails(SnapdragonVRControllerKeys::SnapdragonVRController_Right_SecondaryHandTrigger, LOCTEXT("SnapdragonVRController_Right_SecondaryHandTrigger", "SnapdragonVR Controller (R) Secondary Hand Trigger"), FKeyDetails::GamepadKey));
	EKeys::AddKey(FKeyDetails(SnapdragonVRControllerKeys::SnapdragonVRController_Right_SecondaryThumbstick, LOCTEXT("SnapdragonVRController_Right_SecondaryThumbstick", "SnapdragonVR Controller (R) Secondary Thumbstick"), FKeyDetails::GamepadKey));
	EKeys::AddKey(FKeyDetails(SnapdragonVRControllerKeys::SnapdragonVRController_Right_SecondaryThumbstickUp, LOCTEXT("SnapdragonVRController_Right_SecondaryThumbstickUp", "SnapdragonVR Controller (R)Secondary Thumbstick Up"), FKeyDetails::GamepadKey));
	EKeys::AddKey(FKeyDetails(SnapdragonVRControllerKeys::SnapdragonVRController_Right_SecondaryThumbstickDown, LOCTEXT("SnapdragonVRController_Right_SecondaryThumbstickDown", "SnapdragonVR Controller (R) Secondary Thumbstick Down"), FKeyDetails::GamepadKey));
	EKeys::AddKey(FKeyDetails(SnapdragonVRControllerKeys::SnapdragonVRController_Right_SecondaryThumbstickLeft, LOCTEXT("SnapdragonVRController_Right_SecondaryThumbstickLeft", "SnapdragonVR Controller (R) Secondary Thumbstick Left"), FKeyDetails::GamepadKey));
	EKeys::AddKey(FKeyDetails(SnapdragonVRControllerKeys::SnapdragonVRController_Right_SecondaryThumbstickRight, LOCTEXT("SnapdragonVRController_Right_SecondaryThumbstickRight", "SnapdragonVR Controller (R) Secondary Thumbstick Right"), FKeyDetails::GamepadKey));
	EKeys::AddKey(FKeyDetails(SnapdragonVRControllerKeys::SnapdragonVRController_Right_ButtonUp, LOCTEXT("SnapdragonVRController_Right_ButtonUp", "SnapdragonVR Controller (R) Button Up"), FKeyDetails::GamepadKey));
	EKeys::AddKey(FKeyDetails(SnapdragonVRControllerKeys::SnapdragonVRController_Right_ButtonDown, LOCTEXT("SnapdragonVRController_Right_ButtonDown", "SnapdragonVR Controller (R) Button Down"), FKeyDetails::GamepadKey));
	EKeys::AddKey(FKeyDetails(SnapdragonVRControllerKeys::SnapdragonVRController_Right_ButtonLeft, LOCTEXT("SnapdragonVRController_Right_ButtonLeft", "SnapdragonVR Controller (R) Button Left"), FKeyDetails::GamepadKey));
	EKeys::AddKey(FKeyDetails(SnapdragonVRControllerKeys::SnapdragonVRController_Right_ButtonRight, LOCTEXT("SnapdragonVRController_Right_ButtonRight", "SnapdragonVR Controller (R) Button Right"), FKeyDetails::GamepadKey));


}

void FSnapdragonVRController::ApplicationPauseDelegate()
{
	StopTracking();
	UE_LOG(LogSnapdragonVRController, Log, TEXT("ApplicationPauseDelegate"));
}

void FSnapdragonVRController::ApplicationResumeDelegate()
{
	StartTracking();
	UE_LOG(LogSnapdragonVRController, Log, TEXT("ApplicationResumeDelegate"));
}

void FSnapdragonVRController::StartTracking()
{
#if SNAPDRAGONVRCONTROLLER_SUPPORTED_PLATFORMS

	if (bIsTracking)
		return;

	int trackingDevicesCount = 0;

	for (int i = 0; i < CONTROLLER_PAIRS_MAX; i++)
	{
		for (int j = 0; j < CONTROLLERS_PER_PLAYER; j++)
		{
			ControllerAndHandIndexToDeviceIdMap[i][j] = svrControllerStartTracking(NULL);

			if (ControllerAndHandIndexToDeviceIdMap[i][j] > -1)
				++trackingDevicesCount;

			UE_LOG(LogSnapdragonVRController, Log, TEXT("ControllerAndHandIndexToDeviceIdMap[i][j] = %d"), ControllerAndHandIndexToDeviceIdMap[i][j]);
			FMemory::Memset(&currentState[i][j], 0, sizeof(svrControllerState));
			previousButtonState[i][j] = 0;
			previousTouchState[i][j] = 0;
			previousConnectionState[i][j] = svrControllerConnectionState::kNotInitialized;
		}
	}

	if(trackingDevicesCount > 0)
		bIsTracking = true;

#endif
}

void FSnapdragonVRController::StopTracking()
{
#if SNAPDRAGONVRCONTROLLER_SUPPORTED_PLATFORMS

	if (!bIsTracking)
		return;

	for (int i = 0; i < CONTROLLER_PAIRS_MAX; i++)
	{
		for (int j = 0; j < CONTROLLERS_PER_PLAYER; j++)
		{
			svrControllerStopTracking(ControllerAndHandIndexToDeviceIdMap[i][j]);
			ControllerAndHandIndexToDeviceIdMap[i][j] = -1;
		}
	}

	bIsTracking = false;

#endif
}


void FSnapdragonVRController::PollController()
			{
#if SNAPDRAGONVRCONTROLLER_SUPPORTED_PLATFORMS

	if (!bIsTracking)
				{
		StartTracking();
				}

	for (int i = 0; i < CONTROLLER_PAIRS_MAX; i++)
	{
		for (int j = 0; j < CONTROLLERS_PER_PLAYER; j++)
				{
			previousButtonState[i][j] = currentState[i][j].buttonState;
			previousTouchState[i][j] = currentState[i][j].isTouching;
			previousConnectionState[i][j] = currentState[i][j].connectionState;

			currentState[i][j] = svrControllerGetState(ControllerAndHandIndexToDeviceIdMap[i][j]);
		}
	}

#endif
}

void FSnapdragonVRController::ProcessControllerButtons()
{
#if SNAPDRAGONVRCONTROLLER_SUPPORTED_PLATFORMS

	for (int i = 0; i < CONTROLLER_PAIRS_MAX; i++)
	{
		for (int j = 0; j < CONTROLLERS_PER_PLAYER; j++)
		{
			EControllerHand Hand = static_cast<EControllerHand>(j);

			if (IsAvailable(i, Hand))
			{
				for (int32 ButtonIndex = 0; ButtonIndex < (int32)ESnapdragonVRControllerButton::TotalCount; ++ButtonIndex)
				{
					ESnapdragonVRControllerButton::Type ButtonType = static_cast<ESnapdragonVRControllerButton::Type>(ButtonIndex);

					// Process our known set of buttons
					if (GetButtonDown(i, Hand, ButtonType))
					{
						UE_LOG(LogSnapdragonVRController, Log, TEXT("Button down = %s, HandId = %d, ButtonIndex = %d, mask = %X"), *Buttons[j][ButtonIndex].ToString(), j, ButtonIndex, (1 << ButtonIndex));
						MessageHandler->OnControllerButtonPressed(Buttons[j][ButtonIndex], 0, false);
					}
					else if (GetButtonUp(i, Hand, ButtonType))
					{
						UE_LOG(LogSnapdragonVRController, Log, TEXT("Button up = %s, HandId = %d, ButtonIndex = %d, mask = %X"), *Buttons[j][ButtonIndex].ToString(), j, ButtonIndex, (1 << ButtonIndex));
						MessageHandler->OnControllerButtonReleased(Buttons[j][ButtonIndex], 0, false);
					}
				}

				for (int32 TouchIndex = 0; TouchIndex < (int32)ESnapdragonVRControllerTouch::TotalCount; ++TouchIndex)
				{
					ESnapdragonVRControllerTouch::Type TouchType = static_cast<ESnapdragonVRControllerTouch::Type>(TouchIndex);

					// Process our known set of buttons
					if (GetTouchDown(i, Hand, TouchType))
					{
						UE_LOG(LogSnapdragonVRController, Log, TEXT("Touch down = %s, HandId = %d, TouchIndex = %d, mask = %X"), *Touches[j][TouchIndex].ToString(), j, TouchIndex, (1 << TouchIndex));
						MessageHandler->OnControllerButtonPressed(Touches[j][TouchIndex], 0, false);

						if (TouchType == ESnapdragonVRControllerTouch::PrimaryThumbstick)
						{
							FVector2D Location = GetAxis2D(i, EControllerHand::Left, ESnapdragonVRControllerAxis2D::PrimaryThumbstick);

							if (Hand == EControllerHand::Left)
							{
								MessageHandler->OnControllerAnalog(SnapdragonVRControllerKeyNames::SnapdragonVRController_Left_Axis2DPrimaryThumbstick_X, 0, Location.X);
								MessageHandler->OnControllerAnalog(SnapdragonVRControllerKeyNames::SnapdragonVRController_Left_Axis2DPrimaryThumbstick_Y, 0, Location.Y);
							}
							else
							{
								MessageHandler->OnControllerAnalog(SnapdragonVRControllerKeyNames::SnapdragonVRController_Right_Axis2DPrimaryThumbstick_X, 0, Location.X);
								MessageHandler->OnControllerAnalog(SnapdragonVRControllerKeyNames::SnapdragonVRController_Right_Axis2DPrimaryThumbstick_Y, 0, Location.Y);
							}
						}
						else if(TouchType == ESnapdragonVRControllerTouch::SecondaryThumbstick)
						{
							FVector2D Location = GetAxis2D(i, EControllerHand::Left, ESnapdragonVRControllerAxis2D::SecondaryThumbstick);

							if (Hand == EControllerHand::Left)
							{
								MessageHandler->OnControllerAnalog(SnapdragonVRControllerKeyNames::SnapdragonVRController_Left_Axis2DSecondaryThumbstick_X, 0, Location.X);
								MessageHandler->OnControllerAnalog(SnapdragonVRControllerKeyNames::SnapdragonVRController_Left_Axis2DSecondaryThumbstick_Y, 0, Location.Y);
							}
							else
							{
								MessageHandler->OnControllerAnalog(SnapdragonVRControllerKeyNames::SnapdragonVRController_Right_Axis2DSecondaryThumbstick_X, 0, Location.X);
								MessageHandler->OnControllerAnalog(SnapdragonVRControllerKeyNames::SnapdragonVRController_Right_Axis2DSecondaryThumbstick_Y, 0, Location.Y);
							}
						}

					}
					else if (GetTouchUp(i, Hand, TouchType))
					{
						UE_LOG(LogSnapdragonVRController, Log, TEXT("Touch up = %s, HandId = %d, TouchIndex = %d, mask = %X"), *Touches[j][TouchIndex].ToString(), j, TouchIndex, (1 << TouchIndex));
						MessageHandler->OnControllerButtonReleased(Touches[j][TouchIndex], 0, false);

						if (TouchType == ESnapdragonVRControllerTouch::PrimaryThumbstick)
						{
							if (Hand == EControllerHand::Left)
							{
								MessageHandler->OnControllerAnalog(SnapdragonVRControllerKeyNames::SnapdragonVRController_Left_Axis2DPrimaryThumbstick_X, 0, 0);
								MessageHandler->OnControllerAnalog(SnapdragonVRControllerKeyNames::SnapdragonVRController_Left_Axis2DPrimaryThumbstick_Y, 0, 0);
							}
							else
							{
								MessageHandler->OnControllerAnalog(SnapdragonVRControllerKeyNames::SnapdragonVRController_Right_Axis2DPrimaryThumbstick_X, 0, 0);
								MessageHandler->OnControllerAnalog(SnapdragonVRControllerKeyNames::SnapdragonVRController_Right_Axis2DPrimaryThumbstick_Y, 0, 0);
							}
						}
						else if (TouchType == ESnapdragonVRControllerTouch::SecondaryThumbstick)
						{
							if (Hand == EControllerHand::Left)
							{
								MessageHandler->OnControllerAnalog(SnapdragonVRControllerKeyNames::SnapdragonVRController_Left_Axis2DSecondaryThumbstick_X, 0, 0);
								MessageHandler->OnControllerAnalog(SnapdragonVRControllerKeyNames::SnapdragonVRController_Left_Axis2DSecondaryThumbstick_Y, 0, 0);
							}
							else
							{
								MessageHandler->OnControllerAnalog(SnapdragonVRControllerKeyNames::SnapdragonVRController_Right_Axis2DSecondaryThumbstick_X, 0, 0);
								MessageHandler->OnControllerAnalog(SnapdragonVRControllerKeyNames::SnapdragonVRController_Right_Axis2DSecondaryThumbstick_Y, 0, 0);
							}
						}
					}
				}


				if (Hand == EControllerHand::Left)
				{	
					float Input = GetAxis1D(i, Hand, ESnapdragonVRControllerAxis1D::PrimaryIndexTrigger);
					MessageHandler->OnControllerAnalog(SnapdragonVRControllerKeyNames::SnapdragonVRController_Left_Axis1DPrimaryIndexTrigger, 0, Input);
					
					Input = GetAxis1D(i, Hand, ESnapdragonVRControllerAxis1D::SecondaryIndexTrigger);
					MessageHandler->OnControllerAnalog(SnapdragonVRControllerKeyNames::SnapdragonVRController_Left_SecondaryIndexTrigger, 0, Input);

					Input = GetAxis1D(i, Hand, ESnapdragonVRControllerAxis1D::PrimaryHandTrigger);
					MessageHandler->OnControllerAnalog(SnapdragonVRControllerKeyNames::SnapdragonVRController_Left_PrimaryHandTrigger, 0, Input);

					Input = GetAxis1D(i, Hand, ESnapdragonVRControllerAxis1D::SecondaryHandTrigger);
					MessageHandler->OnControllerAnalog(SnapdragonVRControllerKeyNames::SnapdragonVRController_Left_SecondaryHandTrigger, 0, Input);

				}
				else if (Hand == EControllerHand::Right)
				{	
					float Input = GetAxis1D(i, Hand, ESnapdragonVRControllerAxis1D::PrimaryIndexTrigger);
					MessageHandler->OnControllerAnalog(SnapdragonVRControllerKeyNames::SnapdragonVRController_Right_Axis1DPrimaryIndexTrigger, 0, Input);

					Input = GetAxis1D(i, Hand, ESnapdragonVRControllerAxis1D::SecondaryIndexTrigger);
					MessageHandler->OnControllerAnalog(SnapdragonVRControllerKeyNames::SnapdragonVRController_Right_SecondaryIndexTrigger, 0, Input);

					Input = GetAxis1D(i, Hand, ESnapdragonVRControllerAxis1D::PrimaryHandTrigger);
					MessageHandler->OnControllerAnalog(SnapdragonVRControllerKeyNames::SnapdragonVRController_Right_PrimaryHandTrigger, 0, Input);

					Input = GetAxis1D(i, Hand, ESnapdragonVRControllerAxis1D::SecondaryHandTrigger);
					MessageHandler->OnControllerAnalog(SnapdragonVRControllerKeyNames::SnapdragonVRController_Right_SecondaryHandTrigger, 0, Input);
				}
			}
		}
	}

#endif // SNAPDRAGONVRCONTROLLER_SUPPORTED_PLATFORMS
}

void FSnapdragonVRController::ProcessControllerEvents()
{
#if SNAPDRAGONVRCONTROLLER_SUPPORTED_PLATFORMS

	for (int i = 0; i < CONTROLLER_PAIRS_MAX; i++)
	{
		for (int j = 0; j < CONTROLLERS_PER_PLAYER; j++)
		{
			svrControllerConnectionState currentConnectionState = currentState[i][j].connectionState;
			if (previousConnectionState[i][j] != currentConnectionState)
			{
				ESnapdragonVRControllerState NewState;
				switch (currentConnectionState)
				{
				case svrControllerConnectionState::kDisconnected:
					NewState = ESnapdragonVRControllerState::Disconnected;
					break;
				case svrControllerConnectionState::kConnected:
					NewState = ESnapdragonVRControllerState::Connected;
					break;
				case svrControllerConnectionState::kConnecting:
					NewState = ESnapdragonVRControllerState::Connecting;
					break;
				case svrControllerConnectionState::kError:
					NewState = ESnapdragonVRControllerState::Error;
					break;

				default:
					UE_LOG(LogSnapdragonVRController, Log, TEXT("Connection type not supported: %d"), currentConnectionState);
					NewState = ESnapdragonVRControllerState::Error;
					break;
				}

				EControllerHand Hand = static_cast<EControllerHand>(j);
				USnapdragonVRControllerFunctionLibrary::GetSnapdragonVRControllerEventManager()->OnControllerStateChangedDelegate.Broadcast(i, Hand, NewState);
			}
		}
	}
#endif
}

bool FSnapdragonVRController::IsAvailable(const int ControllerIndex, const EControllerHand DeviceHand) const
{
#if SNAPDRAGONVRCONTROLLER_SUPPORTED_PLATFORMS

	if ((ControllerIndex >= 0 || ControllerIndex < CONTROLLER_PAIRS_MAX) && (int)DeviceHand < CONTROLLERS_PER_PLAYER)
	{
		if (ControllerAndHandIndexToDeviceIdMap[ControllerIndex][ (int32)DeviceHand] >= 0 && 
			currentState[ControllerIndex][(int32)DeviceHand].connectionState == svrControllerConnectionState::kConnected)
		{
			return true;
		}
	}

#endif

	return false;
}

int32_t FSnapdragonVRController::GetBatteryLevel(const int ControllerIndex, const EControllerHand DeviceHand) const
{
#if SNAPDRAGONVRCONTROLLER_SUPPORTED_PLATFORMS

	if (IsAvailable(ControllerIndex, DeviceHand))
	{
		int32_t controllerHandle = ControllerAndHandIndexToDeviceIdMap[ControllerIndex][(int32)DeviceHand];
		int32_t batteryLevel = -1;

		svrControllerQuery(controllerHandle, svrControllerQueryType::kControllerQueryBatteryRemaining, &batteryLevel, sizeof(batteryLevel));

		return batteryLevel;
	}

#endif

	return 0;
}

/** Checks if the controller battery is charging. */
bool FSnapdragonVRController::Recenter(const int ControllerIndex, const EControllerHand DeviceHand) const
{
#if SNAPDRAGONVRCONTROLLER_SUPPORTED_PLATFORMS

	if (IsAvailable(ControllerIndex, DeviceHand))
	{
		int32_t controllerHandle = ControllerAndHandIndexToDeviceIdMap[ControllerIndex][(int32)DeviceHand];
		svrControllerSendMessage(controllerHandle, svrControllerMessageType::kControllerMessageRecenter, 0, 0);
		USnapdragonVRControllerFunctionLibrary::GetSnapdragonVRControllerEventManager()->OnControllerRecenteredDelegate.Broadcast(ControllerIndex, DeviceHand);
		return true;
	}

#endif

	return false;
}

float FSnapdragonVRController::GetWorldToMetersScale() const
{
	float worldScale = 100;

	if (IsInGameThread() && GWorld != nullptr)
	{
		worldScale = GWorld->GetWorldSettings()->WorldToMeters;
	}

	return worldScale;
}

void FSnapdragonVRController::Tick(float DeltaTime)
{
}

void FSnapdragonVRController::SendControllerEvents()
{
	PollController();
	ProcessControllerButtons();
	ProcessControllerEvents();
}

void FSnapdragonVRController::SetMessageHandler(const TSharedRef< FGenericApplicationMessageHandler >& InMessageHandler)
{
	MessageHandler = InMessageHandler;
}

bool FSnapdragonVRController::Exec(UWorld* InWorld, const TCHAR* Cmd, FOutputDevice& Ar)
{
	return false;
}

void FSnapdragonVRController::SetChannelValue(int32 ControllerId, FForceFeedbackChannelType ChannelType, float Value)
{
#if SNAPDRAGONVRCONTROLLER_SUPPORTED_PLATFORMS

	// Skip unless this is the left or right large channel, which we consider to be the only SteamVRController feedback channel
	if (ChannelType != FForceFeedbackChannelType::LEFT_LARGE && ChannelType != FForceFeedbackChannelType::RIGHT_LARGE)
	{
		return;
	}
	
	const EControllerHand Hand = (ChannelType == FForceFeedbackChannelType::LEFT_LARGE) ? EControllerHand::Left : EControllerHand::Right;
	if (IsAvailable(ControllerId, Hand))
	{	
		int32_t controllerHandle = ControllerAndHandIndexToDeviceIdMap[ControllerId][(int32)Hand];
		svrControllerSendMessage(controllerHandle, svrControllerMessageType::kControllerMessageVibration, Value, 1.0f);
	}

#endif // SNAPDRAGONVRCONTROLLER_SUPPORTED_PLATFORMS
}

void FSnapdragonVRController::SetChannelValues(int32 ControllerId, const FForceFeedbackValues& Values)
{
#if SNAPDRAGONVRCONTROLLER_SUPPORTED_PLATFORMS	

	EControllerHand Hand = EControllerHand::Left;
	if (IsAvailable(ControllerId, Hand))
	{
		int32_t controllerHandle = ControllerAndHandIndexToDeviceIdMap[ControllerId][(int32)Hand];
		svrControllerSendMessage(controllerHandle, svrControllerMessageType::kControllerMessageVibration, Values.LeftLarge, 1.0f);
	}

	Hand = EControllerHand::Right;
	if (IsAvailable(ControllerId, Hand))
	{
		int32_t controllerHandle = ControllerAndHandIndexToDeviceIdMap[ControllerId][(int32)Hand];
		svrControllerSendMessage(controllerHandle, svrControllerMessageType::kControllerMessageVibration, Values.RightLarge, 1.0f);
	}

#endif // SNAPDRAGONVRCONTROLLER_SUPPORTED_PLATFORMS
}

bool FSnapdragonVRController::GetControllerOrientationAndPosition(const int32 ControllerIndex, const EControllerHand DeviceHand, FRotator& OutOrientation, FVector& OutPosition, float WorldToMetersScale) const
{
#if SNAPDRAGONVRCONTROLLER_SUPPORTED_PLATFORMS

	if (IsAvailable(ControllerIndex, DeviceHand))
	{
		OutPosition = GetPosition(ControllerIndex, DeviceHand) * WorldToMetersScale;
		OutOrientation = GetOrientation(ControllerIndex, DeviceHand).Rotator();

		return true;
	}

#endif

	return false;
}

ETrackingStatus FSnapdragonVRController::GetControllerTrackingStatus(const int32 ControllerIndex, const EControllerHand DeviceHand) const
{
#if SNAPDRAGONVRCONTROLLER_SUPPORTED_PLATFORMS

	if (IsAvailable(ControllerIndex, DeviceHand))
	{
		return ETrackingStatus::Tracked;
	}

#endif

	return ETrackingStatus::NotTracked;
}

