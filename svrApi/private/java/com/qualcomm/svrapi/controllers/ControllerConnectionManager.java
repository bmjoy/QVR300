//-----------------------------------------------------------------------------
//  Copyright (c) 2017 Qualcomm Technologies, Inc.
//  All Rights Reserved. Qualcomm Technologies Proprietary and Confidential.
//-----------------------------------------------------------------------------

package com.qualcomm.svrapi.controllers;

import android.content.Context;
import android.content.Intent;
import android.util.Log;

import java.lang.ref.WeakReference;
import java.util.Dictionary;
import java.util.Hashtable;

class ControllerConnectionManager {

    static Dictionary<String, ControllerConnection> controllerConnections = new Hashtable<>();

    static ControllerConnection.OnControllerConnectionListener dictionaryUpdater = new ControllerConnection.OnControllerConnectionListener() {

        @Override
        public void OnApiAvailable(Intent intent, IControllerInterface controllerInterface) {

        }

        @Override
        public void OnApiLost(Intent intent) {
            controllerConnections.remove(intent.getPackage());
        }
    };

    /**
     *
     * @param intent
     * @return
     */
    static ControllerConnection Get(Intent intent)
    {
        ControllerConnection inter = null;
        String key = intent.getPackage();
        if( key != null ) {
            inter = controllerConnections.get(key);
        }

        return inter;
    }

    /**
     *
     * @param context
     * @param listener
     * @return
     */
    static boolean RequestConnection(Intent intent, WeakReference<Context> context, ControllerConnection.OnControllerConnectionListener listener) {
        boolean result = false;

        if( intent == null ) {
            return false;
        }

        Log.d(TAG, "Requesting Connection for - " + intent.getPackage());

        //Get the interface for this intent
        ControllerConnection controllerConnection = Get(intent);

        //If no existing entry. create one
        if( controllerConnection == null )
        {
            controllerConnection = new ControllerConnection(intent);
            controllerConnections.put(intent.getPackage(), controllerConnection);
            controllerConnection.AddListener(dictionaryUpdater);
            controllerConnection.AddListener(listener);
            result = controllerConnection.Connect(context);
        }
        else {
            //add the listener to the entry.
            controllerConnection.AddListener(listener);
            //if entry exists
            if(controllerConnection.controllerInterface != null )
            {
                listener.OnApiAvailable(intent, controllerConnection.controllerInterface);
            }
            else {
                //do nothing. already added the listener;
            }

            result = true;
        }

        return result;
    }

    private static final String TAG = "ControllerSelector";
}
