//-----------------------------------------------------------------------------
//  Copyright (c) 2017 Qualcomm Technologies, Inc.
//  All Rights Reserved. Qualcomm Technologies Proprietary and Confidential.
//-----------------------------------------------------------------------------

package com.qualcomm.svrapi.controllers;

import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.os.Message;
import android.util.Log;

import java.lang.ref.WeakReference;

//-------------------------------------------------------------------------
public class ControllerResponseHandler extends Handler {

    private static final String TAG = "ControllerReplyHandler";

    private WeakReference<ControllerManager> controllerManager;

    /**
     *
     * @param ctlMgr
     */
    public ControllerResponseHandler(ControllerManager ctlMgr)
    {
        super(Looper.getMainLooper());
        controllerManager = new WeakReference<>(ctlMgr);
    }

    /**
     *
     * @param msg
     */
    public void handleMessage(Message msg) {

        ControllerManager ctlMgr = controllerManager.get();
        if( ctlMgr != null ) {
            Log.e(TAG, "Handle Message - " + msg.what);
            /**
             * msg.arg1 = service handle
             */
            int handle = msg.arg1;
            long arg1 = 0;
            long arg2 = 0;
            Bundle data = null;

            switch (msg.what) {
                case ControllerManager.CONTROLLER_MEMORY_ALLOCATED: {
                    ControllerContext controllerContext = ctlMgr.listOfControllers.Get(handle);
                    if (controllerContext != null) {
                        arg1 = handle;
                        arg2 = controllerContext.nativeRingBufferPtr;
                        data = null;
                    }
                }
                break;
                case SvrControllerApi.CONTROLLER_CONNECTED:
                case SvrControllerApi.CONTROLLER_DISCONNECTED: {
                    arg1 = handle;
                    arg2 = 0;
                    data = null;
                }
                break;
                case SvrControllerApi.CONTROLLER_ERROR: {
                    arg1 = handle;
                    arg2 = 0;
                    data = null;
                }
                break;
            }


            if (ctlMgr.controllerManagerEventListener != null) {
                ctlMgr.controllerManagerEventListener.OnControllerEvent(msg.what, arg1, arg2, data);
            } else {
                ctlMgr.OnControllerCallback(ctlMgr.nativeHandle, msg.what, arg1, arg2, data);
            }
        }
    }
}
