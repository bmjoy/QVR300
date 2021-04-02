//-----------------------------------------------------------------------------
//  Copyright (c) 2017 Qualcomm Technologies, Inc.
//  All Rights Reserved. Qualcomm Technologies Proprietary and Confidential.
//-----------------------------------------------------------------------------

package com.qualcomm.svrapi;

import android.annotation.SuppressLint;
import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.ServiceConnection;
import android.os.Bundle;
import android.os.Handler;
import android.os.IBinder;
import android.os.Looper;
import android.os.Message;
import android.os.RemoteException;
import android.util.Log;

import com.qualcomm.snapdragonvrservice.ISvrServiceInterface;

import java.lang.ref.WeakReference;
import java.util.ArrayList;
import java.util.List;

@SuppressWarnings("unused")
@SuppressLint("LongLogTag")
/**
 * SvrServiceClient
 */
public class SvrServiceClient
{
    /**
     *
     * @param nativeHandle
     * @param what
     * @param arg1
     * @param arg2
     * @param bundle
     */
    private native void OnServiceCallback(long nativeHandle, int what, int arg1, int arg2, Bundle bundle);

    public interface SvrServiceEventListener {
        void OnSvrServiceEvent(int what, int arg1, int arg2, Bundle bundle);
    }

    long nativeClientHandle;

    /**
     * Service Api to connect to the service
     */
    ISvrServiceInterface svrServiceApi = null;

    /**
     * Reference to the context
     */
    WeakReference<Context> context;

    /**
     * intent to connect to the service
     */
    Intent serviceIntent;

    /**
     *
     */
    SvrServiceEventListener svrServiceEventListener;

    /**
     *
     */
    //-------------------------------------------------------------------------
    protected SvrServiceClient(Context ctxt )
    {
        this(ctxt, null, 0);
    }

    //-------------------------------------------------------------------------
    public SvrServiceClient(Context ctxt, long nativeClientHandle)
    {
        this(ctxt, null, nativeClientHandle);
    }
    /**
     *
     */
    //-------------------------------------------------------------------------
    public SvrServiceClient(Context ctxt, SvrServiceEventListener callback )
    {
        this(ctxt, callback, 0);
    }

    //-------------------------------------------------------------------------
    private SvrServiceClient(Context ctxt, SvrServiceEventListener eventListener, long nativeClientHandle)
    {
        svrServiceResponseHandler = new SvrServiceResponseHandler();
        this.context = new WeakReference<>(ctxt);
        this.svrServiceEventListener = eventListener;
        this.nativeClientHandle = nativeClientHandle;
        serviceIntent = new Intent();
        serviceIntent.setClassName(SVR_SERVICE_PACKAGE_NAME, SVR_SERVICE_CLASS_NAME);
    }

    List<Runnable> listOfPendingActions = new ArrayList<Runnable>();

    //-------------------------------------------------------------------------
    ServiceConnection serviceConnection = new ServiceConnection() {
        @Override
        public void onServiceConnected(ComponentName componentName, IBinder iBinder) {
            Log.e(TAG, "Connection established Client to Service");
            svrServiceApi = ISvrServiceInterface.Stub.asInterface(iBinder);
            svrServiceResponseHandler.sendEmptyMessage(SVRSERVICE_EVENT_CONNECTED);
            svrServiceClientState = State.kSvrServiceClientConnected;

            int size = listOfPendingActions.size();
            for(int i=0;i<size;i++)
            {
                Runnable r = listOfPendingActions.get(i);
                if( r != null ) {
                    r.run();
                }
            }
        }

        @Override
        public void onServiceDisconnected(ComponentName componentName) {
            svrServiceClientState = State.kSvrServiceClientDisconnected;
            svrServiceApi = null;
            svrServiceResponseHandler.sendEmptyMessage(SVRSERVICE_EVENT_DISCONNECTED);
        }
    };

    State svrServiceClientState = State.kSvrServiceClientNotInitialized;

    //-------------------------------------------------------------------------
    public void Connect( )
    {
        if( context.get() != null )
        {
            if( context.get().bindService(serviceIntent, serviceConnection, Context.BIND_AUTO_CREATE) == true )
            {
                svrServiceClientState = State.kSvrServiceClientConnecting;
            }
        }
    }

    //-------------------------------------------------------------------------
    public void Disconnect()
    {
        Context ctxt = context.get();
        if( ctxt != null )
        {
            ctxt.unbindService(serviceConnection);
        }
    }

    //-------------------------------------------------------------------------
    public void ExecuteOnConnection(Runnable runnable) {
        if( svrServiceClientState == State.kSvrServiceClientConnecting )
        {
            listOfPendingActions.add(runnable);
        }
        else {
            if( runnable != null ) {
                runnable.run();
            }
        }
    }

    //-------------------------------------------------------------------------
    public ISvrServiceInterface GetInterface()
    {
        return svrServiceApi;
    }

    //-------------------------------------------------------------------------
    public Intent GetControllerProviderIntent(String desc)
    {
        Intent intent = null;

        if( svrServiceApi != null )
        {
            try {
                intent = svrServiceApi.GetControllerProviderIntent(desc);
            } catch (RemoteException e) {
                e.printStackTrace();
            }
        }

        return intent;
    }

    /**
     * Response Handler from service.
     */
    private SvrServiceResponseHandler svrServiceResponseHandler = null;

    class SvrServiceResponseHandler extends Handler {
        public SvrServiceResponseHandler()
        {
            super(Looper.getMainLooper());
        }
        //---------------------------------------------------------------------
        public void handleMessage(Message msg)
        {
            if( svrServiceEventListener != null ) {
                svrServiceEventListener.OnSvrServiceEvent(msg.what, msg.arg1, msg.arg2, msg.getData());
            }
            else {
                OnServiceCallback(nativeClientHandle, msg.what, msg.arg1, msg.arg2, msg.getData());
            }
        }
    }

    enum State {
        kSvrServiceClientNotInitialized,
        kSvrServiceClientConnecting,
        kSvrServiceClientConnected,
        kSvrServiceClientDisconnected
    }

    private static final int SVRSERVICE_EVENT_CONNECTED     = 0x700;
    private static final int SVRSERVICE_EVENT_DISCONNECTED  = 0x701;

    private static final String SVR_SERVICE_PACKAGE_NAME = "com.qualcomm.snapdragonvrservice";
    private static final String SVR_SERVICE_CLASS_NAME = "com.qualcomm.snapdragonvrservice.SvrService";
    public static final String TAG = "SvrServiceClient";
}
