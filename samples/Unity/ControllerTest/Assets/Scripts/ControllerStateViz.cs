//-----------------------------------------------------------------------------
//  Copyright (c) 2017 Qualcomm Technologies, Inc.
//  All Rights Reserved. Qualcomm Technologies Proprietary and Confidential.
//-----------------------------------------------------------------------------

using System.Collections;
using System.Collections.Generic;
using UnityEngine;

public class ControllerStateViz : MonoBehaviour {
	public GameObject controllerGo;
	public GameObject controllerLeftGo;
	public GameObject touchpad;
	public UnityEngine.UI.Text buttonStateText;
    public UnityEngine.UI.Text batteryLevel;
	public UnityEngine.UI.Image connectionStateIndicator;
	public UnityEngine.UI.Text batteryLevelLeft;
	public UnityEngine.UI.Image connectionStateIndicatorLeft;
    public SvrController controller;
    public SvrController controllerLeft;
	private Transform _ControllerStateObj;
    private SvrControllerCaps controllerCap;
    private SvrControllerCaps controllerLeftCap;
    private int numControllers;
    // Use this for initialization
    void Start () {
        //Debug.Log("ControllerStateViz Start");
		_ControllerStateObj = transform;
        //controller = new SvrController();
        //controllerLeft = new SvrController();
        controllerCap = new SvrControllerCaps();
        controllerLeftCap = new SvrControllerCaps();
        controllerGo.SetActive(false);
        controllerLeftGo.SetActive(false);
        numControllers = 0;
    }

 //   void LateUpdate()
 //   {
 //       Vector3 con0pos = Vector3.zero;
 //       if(controllerCap.caps == 1)
 //       {
 //           _ControllerStateObj.position = SvrManager.Instance.head.transform.position;
 //           _ControllerStateObj.rotation = SvrManager.Instance.head.transform.rotation;
 //       }
 //   }

	/// <summary>
	/// Update this instance.
	/// </summary>
	void Update () {
		{
            string result = "";
            Vector3 con0pos = Vector3.zero;
            batteryLevel.text = "" + controller.BatteryLevel;
			batteryLevelLeft.text = "" + controllerLeft.BatteryLevel;
			//Recenter HeadPose
			{
				if (controller.GetButtonUp (SvrController.svrControllerButton.PrimaryThumbstick)) {
					SvrManager.Instance.RecenterTracking ();
				}
			}

			//Recenter Controller
			{
				if (controller.GetButton (SvrController.svrControllerButton.Back) && controller.GetButtonUp (SvrController.svrControllerButton.Start)) {
					controller.Recenter ();
				}
			}

            if (controller.ConnectionState == SvrController.svrControllerConnectionState.kConnected)
            {
                //Debug.Log("controller is connected");
                if (controllerCap.deviceIdentifier==null)
                {
                    controllerCap = controller.GetCapability;
                    //Debug.Log("Retrieved Capability = " + controllerCap.caps + " ActiveButtons=" + controllerCap.activeButtons + " Device Manufacturer " + controllerCap.deviceManufacturer.ToString() + " Device Identification " + controllerCap.deviceIdentifier.ToString());
                    numControllers++;
                    controllerGo.SetActive(true);
                } 
                /*
                if (controllerCap.deviceIdentifier == null)
                {
                    numControllers++;
                    controllerGo.SetActive(true);
                    controllerCap.deviceIdentifier = "12345";
                }*/
                //Update Connection State
                {
                    Color stateColor = Color.red;

                    switch (controller.ConnectionState)
                    {
                        case SvrController.svrControllerConnectionState.kConnecting:
                            stateColor = Color.yellow;
                            break;
                        case SvrController.svrControllerConnectionState.kConnected:
                            stateColor = Color.green;
                            break;
                        default:
                            stateColor = Color.red;
                            break;
                    }
                    connectionStateIndicator.color = stateColor;
                }
                //Update Orientation
                {
                    if (controllerCap.caps == 1)
                    {
                        controllerGo.transform.localPosition = controller.Position;
                    }
                    controllerGo.transform.localRotation = controller.Orientation;
                }
                //Debug.Log("controllerGo position: " + controller.Position.ToString() + " rotation " + controller.Orientation.ToString());
                //Buttons Pressed
                {
                    foreach (SvrController.svrControllerButton btn in System.Enum.GetValues(typeof(SvrController.svrControllerButton)))
                    {
                        if (controller.GetButton(btn))
                        {
                            if (result.Length != 0)
                            {
                                result += " | ";
                            }
                            result += btn.ToString("g");
                            //Debug.Log("controllerGo buttons " + result);
                        }
                    }
                }
                //Touchpad State
                {
                    if (controller.GetTouch(SvrController.svrControllerTouch.PrimaryThumbstick) == true)
                    {
                        //Debug.Log("Touchpad 1 Active");
                        touchpad.SetActive(true);
                    }
                    else
                    {
                        //Debug.Log("Touchpad 1 Inactive");
                        touchpad.SetActive(!true);
                    }

                    Vector2 tp = controller.GetAxis2D(SvrController.svrControllerAxis2D.PrimaryThumbstick);
                    touchpad.GetComponent<RectTransform>().localPosition = new Vector3(tp.x * 640, tp.y * 640, 1);
                    //Debug.Log("Touchpad 1: tp.x " + tp.x + " tp.y " + tp.y );

                }

            }

            if (controllerLeft.ConnectionState == SvrController.svrControllerConnectionState.kConnected &&
                controllerLeft.GetCapability.deviceIdentifier != controller.GetCapability.deviceIdentifier)
            {
                //Debug.Log("controllerLeft is connected");
                if (controllerLeftCap.deviceIdentifier == null)
                {
                    controllerLeftCap = controllerLeft.GetCapability;
                    //Debug.Log("Retrieved Capability Left = " + controllerLeftCap.caps + " ActiveButtons=" + controllerLeftCap.activeButtons + " Device Manufacturer " + controllerLeftCap.deviceManufacturer.ToString() + " Device Identification " + controllerLeftCap.deviceIdentifier.ToString());
                    numControllers++;
                    controllerLeftGo.SetActive(true);
                }
                /*
                if (controllerLeftCap.deviceIdentifier == null)
                {
                    numControllers++;
                    controllerLeftGo.SetActive(true);
                    controllerLeftCap.deviceIdentifier = "12345";
                }*/
                {
                    Color stateColor = Color.red;
                    switch (controllerLeft.ConnectionState)
                    {
                        case SvrController.svrControllerConnectionState.kConnecting:
                            stateColor = Color.yellow;
                            break;
                        case SvrController.svrControllerConnectionState.kConnected:
                            stateColor = Color.blue;
                            break;
                        default:
                            stateColor = Color.red;
                            break;
                    }
                    connectionStateIndicatorLeft.color = stateColor;
                }
                //Update Orientation
                {
                    if (controllerLeftCap.caps == 1)
                    {
                        controllerLeftGo.transform.localPosition = controllerLeft.Position;
                    }
                    controllerLeftGo.transform.localRotation = controllerLeft.Orientation;
                }
                //Debug.Log("controllerLeftGo position " + controllerLeft.Position.ToString() + " rotation " + controllerLeft.Orientation.ToString());
                //Buttons Pressed
                {

                    foreach (SvrController.svrControllerButton btn in System.Enum.GetValues(typeof(SvrController.svrControllerButton)))
                    {
                        if (controllerLeft.GetButton(btn))
                        {
                            if (result.Length != 0)
                            {
                                result += " | ";
                            }
                            result += btn.ToString("g");
                            //Debug.Log("controllerLeftGo buttons " + result);

                        }
                    }
                }
            }

            buttonStateText.text = result;
		}
	}
}
