//-----------------------------------------------------------------------------
//  Copyright (c) 2017 Qualcomm Technologies, Inc.
//  All Rights Reserved. Qualcomm Technologies Proprietary and Confidential.
//-----------------------------------------------------------------------------

package com.qualcomm.svrapi.controllers;

@SuppressWarnings("unused")
public class SvrControllerApi {

    //! \brief Events to use in svrControllerSendMessage
    public static final int svrControllerMessageType_kControllerMessageRecenter = 0;
    public static final int svrControllerMessageType_kControllerMessageVibration = 1;

    //! \brief Query Values
    public static final int svrControllerQueryType_kControllerQueryBatterRemaining = 0;
    public static final int svrControllerQueryType_kControllerQueryControllerCaps = 1;
    public static final int svrControllerQueryType_kControllerQueryDeviceManufacturer = 2;
    public static final int svrControllerQueryType_kControllerQueryDeviceIdentifier = 3;
    public static final int svrControllerQueryType_kControllerQueryActiveButtons = 4;
    public static final int svrControllerQueryType_kControllerQueryActive2DAnalogs = 5;
    public static final int svrControllerQueryType_kControllerQueryActive1DAnalogs = 6;
    public static final int svrControllerQueryType_kControllerQueryActiveTouchButtons = 7;

    /**
     * Events back from ControllerModule
     * defined in ControllerManager.h
     */
    public static final int CONTROLLER_DISCONNECTED    = 0x601;
    public static final int CONTROLLER_CONNECTED    = 0x602;
    public static final int CONTROLLER_CONNECTING      = 0x603;
    public static final int CONTROLLER_ERROR        = 0x604;
}
