//-----------------------------------------------------------------------------
//  Copyright (c) 2017 Qualcomm Technologies, Inc.
//  All Rights Reserved. Qualcomm Technologies Proprietary and Confidential.
//-----------------------------------------------------------------------------

package com.qualcomm.svrapi.controllers;

import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.ServiceConnection;
import android.os.IBinder;
import android.util.Log;

import java.lang.ref.WeakReference;
import java.util.ArrayList;
import java.util.List;

/**
 *
 */
class ControllerConnection {
    private final static String TAG = "ControllerConnection";
    IControllerInterface controllerInterface;
    List<WeakReference<OnControllerConnectionListener>> listeners = new ArrayList<>();
    Intent intent;
    ServiceConnection serviceConnection = null;

    /**
     *
     * @param intent
     */
    ControllerConnection(final Intent intent)
    {
        this.intent = intent;
        serviceConnection = new ServiceConnection() {
            @Override
            public void onServiceConnected(ComponentName componentName, IBinder iBinder) {
                Log.d(TAG, "Connection established SvrService to Controller Module");
                IControllerInterface controllerInterface = IControllerInterface.Stub.asInterface(iBinder);
                SetApi(controllerInterface);
            }

            @Override
            public void onServiceDisconnected(ComponentName componentName) {
                Log.d(TAG, "Connection lost with controller.");
                SetApi(null);
            }
        };
    }

    /**
     *
     * @param listener
     */
    void AddListener(OnControllerConnectionListener listener)
    {
        listeners.add(new WeakReference<>(listener));
    }

    /**
     *
     */
    void SetApi(IControllerInterface controllerInterface)
    {

        notifyListeners(controllerInterface);
        this.controllerInterface = controllerInterface;
    }

    /**
     *
     */
    private void notifyListeners(IControllerInterface  iInterface) {
        for (int i = 0; i < listeners.size(); i++) {
            WeakReference<OnControllerConnectionListener> listener = listeners.get(i);
            if( listener.get() != null )
            {
                if( iInterface == null )
                {
                    listener.get().OnApiLost(intent);
                }
                else {
                    listener.get().OnApiAvailable(intent, iInterface);
                }
            }
        }
    }

    /**
     *
     * @param context
     * @return
     */
    public boolean Connect(WeakReference<Context> context) {
        boolean result = false;
        if( (context!=null) && (context.get() != null) ) {
            result = context.get().bindService(intent, serviceConnection, Context.BIND_AUTO_CREATE);
        }

        return result;
    }

    /**
     *
     */
    public interface OnControllerConnectionListener {
        void OnApiAvailable(Intent intent, IControllerInterface controllerInterface);
        void OnApiLost(Intent intent);
    }
}
