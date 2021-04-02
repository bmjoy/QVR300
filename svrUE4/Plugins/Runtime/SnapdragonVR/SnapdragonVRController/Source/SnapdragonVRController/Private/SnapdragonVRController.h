//=============================================================================
//
//                  Copyright (c) 2018 QUALCOMM Technologies Inc.
//                              All Rights Reserved.
//
//==============================================================================

#pragma once

#include "IInputDevice.h"
#include "InputCoreTypes.h"
#include "IMotionController.h"
#include "CoreMinimal.h"
#include "ISnapdragonVRControllerPlugin.h"

#if SNAPDRAGONVRCONTROLLER_SUPPORTED_PLATFORMS
#include "svrApi.h"
#endif

/** Total number of controllers in a set */
#define CONTROLLERS_PER_PLAYER	2

//** Total number of controller pairs */
#define CONTROLLER_PAIRS_MAX 1

/** Controller axis mappings. */
#define DOT_45DEG		0.7071f

namespace AndroidControllerKeyNames
{
	const FGamepadKeyNames::Type AndroidMenu("Android_Menu");
	const FGamepadKeyNames::Type AndroidBack("Android_Back");
	const FGamepadKeyNames::Type AndroidVolumeUp("Android_Volume_Up");
	const FGamepadKeyNames::Type AndroidVolumeDown("Android_Volume_Down");
}

namespace SnapdragonVRControllerKeys
{
	// Touch
	const FKey SnapdragonVRController_Left_Touch1("SnapdragonVRController_Left_Touch1");
	const FKey SnapdragonVRController_Left_Touch2("SnapdragonVRController_Left_Touch2");
	const FKey SnapdragonVRController_Left_Touch3("SnapdragonVRController_Left_Touch3");
	const FKey SnapdragonVRController_Left_Touch4("SnapdragonVRController_Left_Touch4");
	const FKey SnapdragonVRController_Left_TouchPrimaryThumbstick("SnapdragonVRController_Left_TouchPrimaryThumbstick");
	const FKey SnapdragonVRController_Left_TouchSecondaryThumbstick("SnapdragonVRController_Left_TouchSecondaryThumbstick");

	const FKey SnapdragonVRController_Right_Touch1("SnapdragonVRController_Right_Touch1");
	const FKey SnapdragonVRController_Right_Touch2("SnapdragonVRController_Right_Touch2");
	const FKey SnapdragonVRController_Right_Touch3("SnapdragonVRController_Right_Touch3");
	const FKey SnapdragonVRController_Right_Touch4("SnapdragonVRController_Right_Touch4");
	const FKey SnapdragonVRController_Right_TouchPrimaryThumbstick("SnapdragonVRController_Right_TouchPrimaryThumbstick");
	const FKey SnapdragonVRController_Right_TouchSecondaryThumbstick("SnapdragonVRController_Right_TouchSecondaryThumbstick");

	// Axis 1D
	const FKey SnapdragonVRController_Left_Axis1DPrimaryIndexTrigger("SnapdragonVRController_Left_Axis1DPrimaryIndexTrigger");
	const FKey SnapdragonVRController_Left_Axis1DSecondaryIndexTrigger("SnapdragonVRController_Left_Axis1DSecondaryIndexTrigger");
	const FKey SnapdragonVRController_Left_Axis1DPrimaryHandTrigger("SnapdragonVRController_Left_Axis1DPrimaryHandTrigger");
	const FKey SnapdragonVRController_Left_Axis1DSecondaryHandTrigger("SnapdragonVRController_Left_Axis1DSecondaryHandTrigger");

	const FKey SnapdragonVRController_Right_Axis1DPrimaryIndexTrigger("SnapdragonVRController_Right_Axis1DPrimaryIndexTrigger");
	const FKey SnapdragonVRController_Right_Axis1DSecondaryIndexTrigger("SnapdragonVRController_Right_Axis1DSecondaryIndexTrigger");
	const FKey SnapdragonVRController_Right_Axis1DPrimaryHandTrigger("SnapdragonVRController_Right_Axis1DPrimaryHandTrigger");
	const FKey SnapdragonVRController_Right_Axis1DSecondaryHandTrigger("SnapdragonVRController_Right_Axis1DSecondaryHandTrigger");

	// Axis 2D
	const FKey SnapdragonVRController_Left_Axis2DPrimaryThumbstick_X("SnapdragonVRController_Left_PrimaryThumbstick_X");
	const FKey SnapdragonVRController_Left_Axis2DPrimaryThumbstick_Y("SnapdragonVRController_Left_PrimaryThumbstick_Y");
	const FKey SnapdragonVRController_Left_Axis2DSecondaryThumbstick_X("SnapdragonVRController_Left_SecondaryThumbstick_X");
	const FKey SnapdragonVRController_Left_Axis2DSecondaryThumbstick_Y("SnapdragonVRController_Left_SecondaryThumbstick_Y");

	const FKey SnapdragonVRController_Right_Axis2DPrimaryThumbstick_X("SnapdragonVRController_Right_PrimaryThumbstick_X");
	const FKey SnapdragonVRController_Right_Axis2DPrimaryThumbstick_Y("SnapdragonVRController_Right_PrimaryThumbstick_Y");
	const FKey SnapdragonVRController_Right_Axis2DSecondaryThumbstick_X("SnapdragonVRController_Right_SecondaryThumbstick_X");
	const FKey SnapdragonVRController_Right_Axis2DSecondaryThumbstick_Y("SnapdragonVRController_Right_SecondaryThumbstick_Y");

	//Buttons
	const FKey SnapdragonVRController_Left_Facebutton1("SnapdragonVRController_Left_Facebutton1");
	const FKey SnapdragonVRController_Left_Facebutton2("SnapdragonVRController_Left_Facebutton2");
	const FKey SnapdragonVRController_Left_Facebutton3("SnapdragonVRController_Left_Facebutton3");
	const FKey SnapdragonVRController_Left_Facebutton4("SnapdragonVRController_Left_Facebutton4");
	const FKey SnapdragonVRController_Left_DPadUp("SnapdragonVRController_Left_DPadUp");
	const FKey SnapdragonVRController_Left_DPadDown("SnapdragonVRController_Left_DPadDown");
	const FKey SnapdragonVRController_Left_DPadLeft("SnapdragonVRController_Left_DPadLeft");
	const FKey SnapdragonVRController_Left_DPadRight("SnapdragonVRController_Left_DPadRight");
	const FKey SnapdragonVRController_Left_Start("SnapdragonVRController_Left_Start");
	const FKey SnapdragonVRController_Left_Back("SnapdragonVRController_Left_Back");
	const FKey SnapdragonVRController_Left_PrimaryShoulder("SnapdragonVRController_Left_PrimaryShoulder");
	const FKey SnapdragonVRController_Left_PrimaryIndexTrigger("SnapdragonVRController_Left_PrimaryIndexTrigger");
	const FKey SnapdragonVRController_Left_PrimaryHandTrigger("SnapdragonVRController_Left_PrimaryHandTrigger");
	const FKey SnapdragonVRController_Left_PrimaryThumbstick("SnapdragonVRController_Left_PrimaryThumbstick");
	const FKey SnapdragonVRController_Left_PrimaryThumbstickUp("SnapdragonVRController_Left_PrimaryThumbstickUp");
	const FKey SnapdragonVRController_Left_PrimaryThumbstickDown("SnapdragonVRController_Left_PrimaryThumbstickDown");
	const FKey SnapdragonVRController_Left_PrimaryThumbstickLeft("SnapdragonVRController_Left_PrimaryThumbstickLeft");
	const FKey SnapdragonVRController_Left_PrimaryThumbstickRight("SnapdragonVRController_Left_PrimaryThumbstickRight");
	const FKey SnapdragonVRController_Left_SecondaryShoulder("SnapdragonVRController_Left_SecondaryShoulder");
	const FKey SnapdragonVRController_Left_SecondaryIndexTrigger("SnapdragonVRController_Left_SecondaryIndexTrigger");
	const FKey SnapdragonVRController_Left_SecondaryHandTrigger("SnapdragonVRController_Left_SecondaryHandTrigger");
	const FKey SnapdragonVRController_Left_SecondaryThumbstick("SnapdragonVRController_Left_SecondaryThumbstick");
	const FKey SnapdragonVRController_Left_SecondaryThumbstickUp("SnapdragonVRController_Left_SecondaryThumbstickUp");
	const FKey SnapdragonVRController_Left_SecondaryThumbstickDown("SnapdragonVRController_Left_SecondaryThumbstickDown");
	const FKey SnapdragonVRController_Left_SecondaryThumbstickLeft("SnapdragonVRController_Left_SecondaryThumbstickLeft");
	const FKey SnapdragonVRController_Left_SecondaryThumbstickRight("SnapdragonVRController_Left_SecondaryThumbstickRight");
	const FKey SnapdragonVRController_Left_ButtonUp("SnapdragonVRController_Left_ButtonUp");
	const FKey SnapdragonVRController_Left_ButtonDown("SnapdragonVRController_Left_ButtonDown");
	const FKey SnapdragonVRController_Left_ButtonLeft("SnapdragonVRController_Left_ButtonLeft");
	const FKey SnapdragonVRController_Left_ButtonRight("SnapdragonVRController_Left_ButtonRight");

	const FKey SnapdragonVRController_Right_Facebutton1("SnapdragonVRController_Right_Facebutton1");
	const FKey SnapdragonVRController_Right_Facebutton2("SnapdragonVRController_Right_Facebutton2");
	const FKey SnapdragonVRController_Right_Facebutton3("SnapdragonVRController_Right_Facebutton3");
	const FKey SnapdragonVRController_Right_Facebutton4("SnapdragonVRController_Right_Facebutton4");
	const FKey SnapdragonVRController_Right_DPadUp("SnapdragonVRController_Right_DPadUp");
	const FKey SnapdragonVRController_Right_DPadDown("SnapdragonVRController_Right_DPadDown");
	const FKey SnapdragonVRController_Right_DPadLeft("SnapdragonVRController_Right_DPadLeft");
	const FKey SnapdragonVRController_Right_DPadRight("SnapdragonVRController_Right_DPadRight");
	const FKey SnapdragonVRController_Right_Start("SnapdragonVRController_Right_Start");
	const FKey SnapdragonVRController_Right_Back("SnapdragonVRController_Right_Back");
	const FKey SnapdragonVRController_Right_PrimaryShoulder("SnapdragonVRController_Right_PrimaryShoulder");
	const FKey SnapdragonVRController_Right_PrimaryIndexTrigger("SnapdragonVRController_Right_PrimaryIndexTrigger");
	const FKey SnapdragonVRController_Right_PrimaryHandTrigger("SnapdragonVRController_Right_PrimaryHandTrigger");
	const FKey SnapdragonVRController_Right_PrimaryThumbstick("SnapdragonVRController_Right_PrimaryThumbstick");
	const FKey SnapdragonVRController_Right_PrimaryThumbstickUp("SnapdragonVRController_Right_PrimaryThumbstickUp");
	const FKey SnapdragonVRController_Right_PrimaryThumbstickDown("SnapdragonVRController_Right_PrimaryThumbstickDown");
	const FKey SnapdragonVRController_Right_PrimaryThumbstickLeft("SnapdragonVRController_Right_PrimaryThumbstickLeft");
	const FKey SnapdragonVRController_Right_PrimaryThumbstickRight("SnapdragonVRController_Right_PrimaryThumbstickRight");
	const FKey SnapdragonVRController_Right_SecondaryShoulder("SnapdragonVRController_Right_SecondaryShoulder");
	const FKey SnapdragonVRController_Right_SecondaryIndexTrigger("SnapdragonVRController_Right_SecondaryIndexTrigger");
	const FKey SnapdragonVRController_Right_SecondaryHandTrigger("SnapdragonVRController_Right_SecondaryHandTrigger");
	const FKey SnapdragonVRController_Right_SecondaryThumbstick("SnapdragonVRController_Right_SecondaryThumbstick");
	const FKey SnapdragonVRController_Right_SecondaryThumbstickUp("SnapdragonVRController_Right_SecondaryThumbstickUp");
	const FKey SnapdragonVRController_Right_SecondaryThumbstickDown("SnapdragonVRController_Right_SecondaryThumbstickDown");
	const FKey SnapdragonVRController_Right_SecondaryThumbstickLeft("SnapdragonVRController_Right_SecondaryThumbstickLeft");
	const FKey SnapdragonVRController_Right_SecondaryThumbstickRight("SnapdragonVRController_Right_SecondaryThumbstickRight");
	const FKey SnapdragonVRController_Right_ButtonUp("SnapdragonVRController_Right_ButtonUp");
	const FKey SnapdragonVRController_Right_ButtonDown("SnapdragonVRController_Right_ButtonDown");
	const FKey SnapdragonVRController_Right_ButtonLeft("SnapdragonVRController_Right_ButtonLeft");
	const FKey SnapdragonVRController_Right_ButtonRight("SnapdragonVRController_Right_ButtonRight");
}

namespace SnapdragonVRControllerKeyNames
{
	// Touch
	const FGamepadKeyNames::Type SnapdragonVRController_Left_Touch1("SnapdragonVRController_Left_Touch1");
	const FGamepadKeyNames::Type SnapdragonVRController_Left_Touch2("SnapdragonVRController_Left_Touch2");
	const FGamepadKeyNames::Type SnapdragonVRController_Left_Touch3("SnapdragonVRController_Left_Touch3");
	const FGamepadKeyNames::Type SnapdragonVRController_Left_Touch4("SnapdragonVRController_Left_Touch4");
	const FGamepadKeyNames::Type SnapdragonVRController_Left_TouchPrimaryThumbstick("SnapdragonVRController_Left_TouchPrimaryThumbstick");
	const FGamepadKeyNames::Type SnapdragonVRController_Left_TouchSecondaryThumbstick("SnapdragonVRController_Left_TouchSecondaryThumbstick");

	const FGamepadKeyNames::Type SnapdragonVRController_Right_Touch1("SnapdragonVRController_Right_Touch1");
	const FGamepadKeyNames::Type SnapdragonVRController_Right_Touch2("SnapdragonVRController_Right_Touch2");
	const FGamepadKeyNames::Type SnapdragonVRController_Right_Touch3("SnapdragonVRController_Right_Touch3");
	const FGamepadKeyNames::Type SnapdragonVRController_Right_Touch4("SnapdragonVRController_Right_Touch4");
	const FGamepadKeyNames::Type SnapdragonVRController_Right_TouchPrimaryThumbstick("SnapdragonVRController_Right_TouchPrimaryThumbstick");
	const FGamepadKeyNames::Type SnapdragonVRController_Right_TouchSecondaryThumbstick("SnapdragonVRController_Right_TouchSecondaryThumbstick");

	// Axis 1D
	const FGamepadKeyNames::Type SnapdragonVRController_Left_Axis1DPrimaryIndexTrigger("SnapdragonVRController_Left_Axis1DPrimaryIndexTrigger");
	const FGamepadKeyNames::Type SnapdragonVRController_Left_Axis1DSecondaryIndexTrigger("SnapdragonVRController_Left_Axis1DSecondaryIndexTrigger");
	const FGamepadKeyNames::Type SnapdragonVRController_Left_Axis1DPrimaryHandTrigger("SnapdragonVRController_Left_Axis1DPrimaryHandTrigger");
	const FGamepadKeyNames::Type SnapdragonVRController_Left_Axis1DSecondaryHandTrigger("SnapdragonVRController_Left_Axis1DSecondaryHandTrigger");

	const FGamepadKeyNames::Type SnapdragonVRController_Right_Axis1DPrimaryIndexTrigger("SnapdragonVRController_Right_Axis1DPrimaryIndexTrigger");
	const FGamepadKeyNames::Type SnapdragonVRController_Right_Axis1DSecondaryIndexTrigger("SnapdragonVRController_Right_Axis1DSecondaryIndexTrigger");
	const FGamepadKeyNames::Type SnapdragonVRController_Right_Axis1DPrimaryHandTrigger("SnapdragonVRController_Right_Axis1DPrimaryHandTrigger");
	const FGamepadKeyNames::Type SnapdragonVRController_Right_Axis1DSecondaryHandTrigger("SnapdragonVRController_Right_Axis1DSecondaryHandTrigger");

	// Axis 2D
	const FGamepadKeyNames::Type SnapdragonVRController_Left_Axis2DPrimaryThumbstick_X("SnapdragonVRController_Left_PrimaryThumbstick_X");
	const FGamepadKeyNames::Type SnapdragonVRController_Left_Axis2DPrimaryThumbstick_Y("SnapdragonVRController_Left_PrimaryThumbstick_Y");
	const FGamepadKeyNames::Type SnapdragonVRController_Left_Axis2DSecondaryThumbstick_X("SnapdragonVRController_Left_SecondaryThumbstick_X");
	const FGamepadKeyNames::Type SnapdragonVRController_Left_Axis2DSecondaryThumbstick_Y("SnapdragonVRController_Left_SecondaryThumbstick_Y");
	const FGamepadKeyNames::Type SnapdragonVRController_Right_Axis2DPrimaryThumbstick_X("SnapdragonVRController_Right_PrimaryThumbstick_X");
	const FGamepadKeyNames::Type SnapdragonVRController_Right_Axis2DPrimaryThumbstick_Y("SnapdragonVRController_Right_PrimaryThumbstick_Y");
	const FGamepadKeyNames::Type SnapdragonVRController_Right_Axis2DSecondaryThumbstick_X("SnapdragonVRController_Right_SecondaryThumbstick_X");
	const FGamepadKeyNames::Type SnapdragonVRController_Right_Axis2DSecondaryThumbstick_Y("SnapdragonVRController_Right_SecondaryThumbstick_Y");

	//Buttons
	const FGamepadKeyNames::Type SnapdragonVRController_Left_Facebutton1("SnapdragonVRController_Left_Facebutton1");
	const FGamepadKeyNames::Type SnapdragonVRController_Left_Facebutton2("SnapdragonVRController_Left_Facebutton2");
	const FGamepadKeyNames::Type SnapdragonVRController_Left_Facebutton3("SnapdragonVRController_Left_Facebutton3");
	const FGamepadKeyNames::Type SnapdragonVRController_Left_Facebutton4("SnapdragonVRController_Left_Facebutton4");
	const FGamepadKeyNames::Type SnapdragonVRController_Left_DPadUp("SnapdragonVRController_Left_DPadUp");
	const FGamepadKeyNames::Type SnapdragonVRController_Left_DPadDown("SnapdragonVRController_Left_DPadDown");
	const FGamepadKeyNames::Type SnapdragonVRController_Left_DPadLeft("SnapdragonVRController_Left_DPadLeft");
	const FGamepadKeyNames::Type SnapdragonVRController_Left_DPadRight("SnapdragonVRController_Left_DPadRight");
	const FGamepadKeyNames::Type SnapdragonVRController_Left_Start("SnapdragonVRController_Left_Start");
	const FGamepadKeyNames::Type SnapdragonVRController_Left_Back("SnapdragonVRController_Left_Back");
	const FGamepadKeyNames::Type SnapdragonVRController_Left_PrimaryShoulder("SnapdragonVRController_Left_PrimaryShoulder");
	const FGamepadKeyNames::Type SnapdragonVRController_Left_PrimaryIndexTrigger("SnapdragonVRController_Left_PrimaryIndexTrigger");
	const FGamepadKeyNames::Type SnapdragonVRController_Left_PrimaryHandTrigger("SnapdragonVRController_Left_PrimaryHandTrigger");
	const FGamepadKeyNames::Type SnapdragonVRController_Left_PrimaryThumbstick("SnapdragonVRController_Left_PrimaryThumbstick");
	const FGamepadKeyNames::Type SnapdragonVRController_Left_PrimaryThumbstickUp("SnapdragonVRController_Left_PrimaryThumbstickUp");
	const FGamepadKeyNames::Type SnapdragonVRController_Left_PrimaryThumbstickDown("SnapdragonVRController_Left_PrimaryThumbstickDown");
	const FGamepadKeyNames::Type SnapdragonVRController_Left_PrimaryThumbstickLeft("SnapdragonVRController_Left_PrimaryThumbstickLeft");	
	const FGamepadKeyNames::Type SnapdragonVRController_Left_PrimaryThumbstickRight("SnapdragonVRController_Left_PrimaryThumbstickRight");
	const FGamepadKeyNames::Type SnapdragonVRController_Left_SecondaryShoulder("SnapdragonVRController_Left_SecondaryShoulder");
	const FGamepadKeyNames::Type SnapdragonVRController_Left_SecondaryIndexTrigger("SnapdragonVRController_Left_SecondaryIndexTrigger");
	const FGamepadKeyNames::Type SnapdragonVRController_Left_SecondaryHandTrigger("SnapdragonVRController_Left_SecondaryHandTrigger");
	const FGamepadKeyNames::Type SnapdragonVRController_Left_SecondaryThumbstick("SnapdragonVRController_Left_SecondaryThumbstick");
	const FGamepadKeyNames::Type SnapdragonVRController_Left_SecondaryThumbstickUp("SnapdragonVRController_Left_SecondaryThumbstickUp");
	const FGamepadKeyNames::Type SnapdragonVRController_Left_SecondaryThumbstickDown("SnapdragonVRController_Left_SecondaryThumbstickDown");
	const FGamepadKeyNames::Type SnapdragonVRController_Left_SecondaryThumbstickLeft("SnapdragonVRController_Left_SecondaryThumbstickLeft");
	const FGamepadKeyNames::Type SnapdragonVRController_Left_SecondaryThumbstickRight("SnapdragonVRController_Left_SecondaryThumbstickRight");
	const FGamepadKeyNames::Type SnapdragonVRController_Left_ButtonUp("SnapdragonVRController_Left_ButtonUp");
	const FGamepadKeyNames::Type SnapdragonVRController_Left_ButtonDown("SnapdragonVRController_Left_ButtonDown");
	const FGamepadKeyNames::Type SnapdragonVRController_Left_ButtonLeft("SnapdragonVRController_Left_ButtonLeft");
	const FGamepadKeyNames::Type SnapdragonVRController_Left_ButtonRight("SnapdragonVRController_Left_ButtonRight");

	const FGamepadKeyNames::Type SnapdragonVRController_Right_Facebutton1("SnapdragonVRController_Right_Facebutton1");
	const FGamepadKeyNames::Type SnapdragonVRController_Right_Facebutton2("SnapdragonVRController_Right_Facebutton2");
	const FGamepadKeyNames::Type SnapdragonVRController_Right_Facebutton3("SnapdragonVRController_Right_Facebutton3");
	const FGamepadKeyNames::Type SnapdragonVRController_Right_Facebutton4("SnapdragonVRController_Right_Facebutton4");
	const FGamepadKeyNames::Type SnapdragonVRController_Right_DPadUp("SnapdragonVRController_Right_DPadUp");
	const FGamepadKeyNames::Type SnapdragonVRController_Right_DPadDown("SnapdragonVRController_Right_DPadDown");
	const FGamepadKeyNames::Type SnapdragonVRController_Right_DPadLeft("SnapdragonVRController_Right_DPadLeft");
	const FGamepadKeyNames::Type SnapdragonVRController_Right_DPadRight("SnapdragonVRController_Right_DPadRight");
	const FGamepadKeyNames::Type SnapdragonVRController_Right_Start("SnapdragonVRController_Right_Start");
	const FGamepadKeyNames::Type SnapdragonVRController_Right_Back("SnapdragonVRController_Right_Back");
	const FGamepadKeyNames::Type SnapdragonVRController_Right_PrimaryShoulder("SnapdragonVRController_Right_PrimaryShoulder");
	const FGamepadKeyNames::Type SnapdragonVRController_Right_PrimaryIndexTrigger("SnapdragonVRController_Right_PrimaryIndexTrigger");
	const FGamepadKeyNames::Type SnapdragonVRController_Right_PrimaryHandTrigger("SnapdragonVRController_Right_PrimaryHandTrigger");
	const FGamepadKeyNames::Type SnapdragonVRController_Right_PrimaryThumbstick("SnapdragonVRController_Right_PrimaryThumbstick");
	const FGamepadKeyNames::Type SnapdragonVRController_Right_PrimaryThumbstickUp("SnapdragonVRController_Right_PrimaryThumbstickUp");
	const FGamepadKeyNames::Type SnapdragonVRController_Right_PrimaryThumbstickDown("SnapdragonVRController_Right_PrimaryThumbstickDown");
	const FGamepadKeyNames::Type SnapdragonVRController_Right_PrimaryThumbstickLeft("SnapdragonVRController_Right_PrimaryThumbstickLeft");
	const FGamepadKeyNames::Type SnapdragonVRController_Right_PrimaryThumbstickRight("SnapdragonVRController_Right_PrimaryThumbstickRight");
	const FGamepadKeyNames::Type SnapdragonVRController_Right_SecondaryShoulder("SnapdragonVRController_Right_SecondaryShoulder");
	const FGamepadKeyNames::Type SnapdragonVRController_Right_SecondaryIndexTrigger("SnapdragonVRController_Right_SecondaryIndexTrigger");
	const FGamepadKeyNames::Type SnapdragonVRController_Right_SecondaryHandTrigger("SnapdragonVRController_Right_SecondaryHandTrigger");
	const FGamepadKeyNames::Type SnapdragonVRController_Right_SecondaryThumbstick("SnapdragonVRController_Right_SecondaryThumbstick");
	const FGamepadKeyNames::Type SnapdragonVRController_Right_SecondaryThumbstickUp("SnapdragonVRController_Right_SecondaryThumbstickUp");
	const FGamepadKeyNames::Type SnapdragonVRController_Right_SecondaryThumbstickDown("SnapdragonVRController_Right_SecondaryThumbstickDown");
	const FGamepadKeyNames::Type SnapdragonVRController_Right_SecondaryThumbstickLeft("SnapdragonVRController_Right_SecondaryThumbstickLeft");
	const FGamepadKeyNames::Type SnapdragonVRController_Right_SecondaryThumbstickRight("SnapdragonVRController_Right_SecondaryThumbstickRight");
	const FGamepadKeyNames::Type SnapdragonVRController_Right_ButtonUp("SnapdragonVRController_Right_ButtonUp");
	const FGamepadKeyNames::Type SnapdragonVRController_Right_ButtonDown("SnapdragonVRController_Right_ButtonDown");
	const FGamepadKeyNames::Type SnapdragonVRController_Right_ButtonLeft("SnapdragonVRController_Right_ButtonLeft");
	const FGamepadKeyNames::Type SnapdragonVRController_Right_ButtonRight("SnapdragonVRController_Right_ButtonRight");

}

class FSnapdragonVRController : public IInputDevice, public IMotionController
{
public:
	FSnapdragonVRController(const TSharedRef< FGenericApplicationMessageHandler >& InMessageHandler);

	virtual ~FSnapdragonVRController();

	static void PreInit();

public:
	struct ESnapdragonVRControllerTouch
	{
		enum Type
		{
			One,
			Two,
			Three,
			Four,
			PrimaryThumbstick,
			SecondaryThumbstick,

			TotalCount
		};
	};

	// Must match the enum order of ESnapdragonVRControllerTouch
	const uint32 KTouchMask[30] =
	{
		0x00000001,
		0x00000002,
		0x00000004,
		0x00000008,
		0x00000010,
		0x00000020
	};

	struct ESnapdragonVRControllerAxis1D
	{	
		enum Type
		{
			PrimaryIndexTrigger,
			SecondaryIndexTrigger,
			PrimaryHandTrigger,
			SecondaryHandTrigger,

			TotalCount
		};
	};

	struct ESnapdragonVRControllerAxis2D
	{
		enum Type
		{
			PrimaryThumbstick,
			SecondaryThumbstick,

			TotalCount
		};
	};

	struct ESnapdragonVRControllerButton
	{
		enum Type
		{
			One,
			Two,
			Three,
			Four,
			DpadUp,
			DpadDown,
			DpadLeft,
			DpadRight,
			Start,
			Back,
			PrimaryShoulder,
			PrimaryIndexTrigger,
			PrimaryHandTrigger,
			PrimaryThumbstick,
			PrimaryThumbstickUp,
			PrimaryThumbstickDown,
			PrimaryThumbstickLeft,
			PrimaryThumbstickRight,
			SecondaryShoulder,
			SecondaryIndexTrigger,
			SecondaryHandTrigger,
			SecondaryThumbstick,
			SecondaryThumbstickUp,
			SecondaryThumbstickDown,
			SecondaryThumbstickLeft,
			SecondaryThumbstickRight,
			Up,
			Down,
			Left,
			Right,

			/** Max number of controller buttons.  Must be < 256 */
			TotalCount
		};
	};

	// Must match the enum order of ESnapdragonVRControllerButton
	const uint32 KButtonMask[30] =
	{
		0x00000001,
		0x00000002,
		0x00000004,
		0x00000008,
		0x00000010,
		0x00000020,
		0x00000040,
		0x00000080,
		0x00000100,
		0x00000200,
		0x00001000,
		0x00002000,
		0x00004000,
		0x00008000,
		0x00010000,
		0x00020000,
		0x00040000,
		0x00080000,
		0x00100000,
		0x00200000,
		0x00400000,
		0x00800000,
		0x01000000,
		0x02000000,
		0x04000000,
		0x08000000,
		0x10000000,
		0x20000000,
		0x40000000,
		0x80000000
	};

public: // Helper Functions

	/** Called beforeg application enters background */
	void ApplicationPauseDelegate();

	/** Called after application resumes */
	void ApplicationResumeDelegate();

	/** Polls the controller state */
	void PollController();

	/** Processes the controller buttons */
	void ProcessControllerButtons();

	/** Processes the controller events */
	void ProcessControllerEvents();

	/** Checks if the controller is available */
	bool IsAvailable(int ControllerIndex, const EControllerHand DeviceHand) const;

	/** Checks if the controller battery is charging. */
	int32_t GetBatteryLevel(const int ControllerIndex, const EControllerHand DeviceHand) const;

	/** Recenters the controller */
	bool Recenter(const int ControllerIndex, const EControllerHand DeviceHand) const;

	float GetWorldToMetersScale() const;

private:

#if SNAPDRAGONVRCONTROLLER_SUPPORTED_PLATFORMS
	inline bool GetButton(const int32 ControllerIndex, const EControllerHand DeviceHand, ESnapdragonVRControllerButton::Type button) const 
	{
		uint32 mask = KButtonMask[static_cast<uint32>(button)];
		return ((currentState[ControllerIndex][(int32)DeviceHand].buttonState & mask) != 0);
	}

	inline bool GetButtonUp(const int32 ControllerIndex, const EControllerHand DeviceHand, ESnapdragonVRControllerButton::Type button) const 
	{
		uint32 mask = KButtonMask[static_cast<uint32>(button)];
		return ((previousButtonState[ControllerIndex][(int32)DeviceHand] & mask) != 0) && ((currentState[ControllerIndex][(int32)DeviceHand].buttonState & mask) == 0);
	}

	inline bool GetButtonDown(const int32 ControllerIndex, const EControllerHand DeviceHand, ESnapdragonVRControllerButton::Type button) const
	{
		uint32 mask = KButtonMask[static_cast<uint32>(button)];
		return ((previousButtonState[ControllerIndex][(int32)DeviceHand] & mask) == 0) && ((currentState[ControllerIndex][(int32)DeviceHand].buttonState & mask) != 0);
	}

	inline FQuat GetOrientation(const int32 ControllerIndex, const EControllerHand DeviceHand) const
	{		
		svrQuaternion rotation = currentState[ControllerIndex][(int32)DeviceHand].rotation;
		return FQuat(rotation.z, rotation.x, rotation.y, rotation.w);
	}

	inline FVector GetPosition(const int32 ControllerIndex, const EControllerHand DeviceHand) const
	{		
		svrVector3 position = currentState[ControllerIndex][(int32)DeviceHand].position;
		return FVector(position.x, position.z, position.y);
	}

	inline long GetTimestamp(const int32 ControllerIndex, const EControllerHand DeviceHand) const
	{
		return currentState[ControllerIndex][(int32)DeviceHand].timestamp;
	}

	inline FVector2D GetAxis2D(const int32 ControllerIndex, const EControllerHand DeviceHand, ESnapdragonVRControllerAxis2D::Type axi2D) const
	{
		svrVector2 axis = currentState[ControllerIndex][(int32)DeviceHand].analog2D[(int)axi2D];
		return FVector2D(axis.x, axis.y);
	}

	inline float GetAxis1D(const int32 ControllerIndex, const EControllerHand DeviceHand, ESnapdragonVRControllerAxis1D::Type axis1D) const
	{
		return currentState[ControllerIndex][(int32)DeviceHand].analog1D[(int)axis1D];
	}

	inline bool GetTouch(const int32 ControllerIndex, const EControllerHand DeviceHand, ESnapdragonVRControllerTouch::Type touch) const
	{
		uint32 mask = KTouchMask[static_cast<uint32>(touch)];
		return ((currentState[ControllerIndex][(int32)DeviceHand].isTouching & mask) != 0);
	}

	inline bool GetTouchDown(const int32 ControllerIndex, const EControllerHand DeviceHand, ESnapdragonVRControllerTouch::Type touch) const
	{
		uint32 mask = KTouchMask[static_cast<uint32>(touch)];
		return ((previousTouchState[ControllerIndex][(int32)DeviceHand] & mask) == 0) && ((currentState[ControllerIndex][(int32)DeviceHand].isTouching & mask) != 0);
	}

	inline bool GetTouchUp(const int32 ControllerIndex, const EControllerHand DeviceHand, ESnapdragonVRControllerTouch::Type touch) const
	{
		uint32 mask = KTouchMask[static_cast<uint32>(touch)];
		return ((previousTouchState[ControllerIndex][(int32)DeviceHand] & mask) != 0) && ((currentState[ControllerIndex][(int32)DeviceHand].isTouching & mask) == 0);
	}
#endif

	void StartTracking();
	void StopTracking();

public:	// IInputDevice

	/** Tick the interface (e.g. check for new controllers) */
	virtual void Tick(float DeltaTime);

	/** Poll for controller state and send events if needed */
	virtual void SendControllerEvents();

	/** Set which MessageHandler will get the events from SendControllerEvents. */
	virtual void SetMessageHandler(const TSharedRef< FGenericApplicationMessageHandler >& InMessageHandler);

	/** Exec handler to allow console commands to be passed through for debugging */
	virtual bool Exec(UWorld* InWorld, const TCHAR* Cmd, FOutputDevice& Ar);

	/** IForceFeedbackSystem */
	virtual void SetChannelValue(int32 ControllerId, FForceFeedbackChannelType ChannelType, float Value);
	virtual void SetChannelValues(int32 ControllerId, const FForceFeedbackValues &values);

public: // IMotionController

	static FName DeviceTypeName;
	virtual FName GetMotionControllerDeviceTypeName() const override
	{
		return DeviceTypeName;
	}

	/**
	* Returns the calibration-space orientation of the requested controller's hand.
	*
	* @param ControllerIndex	The Unreal controller (player) index of the contoller set
	* @param DeviceHand		Which hand, within the controller set for the player, to get the orientation and position for
	* @param OutOrientation	(out) If tracked, the orientation (in calibrated-space) of the controller in the specified hand
	* @param OutPosition		(out) If tracked, the position (in calibrated-space) of the controller in the specified hand
	* @return					True if the device requested is valid and tracked, false otherwise
	*/
	virtual bool GetControllerOrientationAndPosition(const int32 ControllerIndex, const EControllerHand DeviceHand, FRotator& OutOrientation, FVector& OutPosition, float WorldToMetersScale) const;

	/**
	* Returns the tracking status (e.g. not tracked, intertial-only, fully tracked) of the specified controller
	*
	* @return	Tracking status of the specified controller, or ETrackingStatus::NotTracked if the device is not found
	*/
	virtual ETrackingStatus GetControllerTrackingStatus(const int32 ControllerIndex, const EControllerHand DeviceHand) const;

	/** Cached controller info */
#if SNAPDRAGONVRCONTROLLER_SUPPORTED_PLATFORMS
	svrControllerState currentState[CONTROLLER_PAIRS_MAX][CONTROLLERS_PER_PLAYER];
	uint32_t previousButtonState[CONTROLLER_PAIRS_MAX][CONTROLLERS_PER_PLAYER];
	uint32_t previousTouchState[CONTROLLER_PAIRS_MAX][CONTROLLERS_PER_PLAYER];
	svrControllerConnectionState previousConnectionState[CONTROLLER_PAIRS_MAX][CONTROLLERS_PER_PLAYER];
#endif

private:

	bool bIsTracking;
	
	/** Mapping of controller buttons and touch */
	FGamepadKeyNames::Type Buttons[CONTROLLERS_PER_PLAYER][ESnapdragonVRControllerButton::TotalCount];
	FGamepadKeyNames::Type Touches[CONTROLLERS_PER_PLAYER][ESnapdragonVRControllerTouch::TotalCount];

	int32_t ControllerAndHandIndexToDeviceIdMap[CONTROLLER_PAIRS_MAX][CONTROLLERS_PER_PLAYER];

	/** handler to send all messages to */
	TSharedRef<FGenericApplicationMessageHandler> MessageHandler;

	/** Last Orientation used */
	mutable FRotator LastOrientation;
};
