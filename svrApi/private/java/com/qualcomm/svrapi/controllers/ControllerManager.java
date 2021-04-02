//-----------------------------------------------------------------------------
//  Copyright (c) 2017 Qualcomm Technologies, Inc.
//  All Rights Reserved. Qualcomm Technologies Proprietary and Confidential.
//-----------------------------------------------------------------------------

package com.qualcomm.svrapi.controllers;

import android.content.Context;
import android.content.Intent;
import android.os.Bundle;
import android.os.Message;
import android.os.RemoteException;
import android.util.Log;

import com.qualcomm.svrapi.SvrServiceClient;

import java.lang.ref.WeakReference;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;

@SuppressWarnings("unused")
public class ControllerManager {

    /**
     *
     * @param nativeHandle
     * @param what
     * @param arg1
     * @param arg2
     * @param bundle
     */
    @SuppressWarnings("all")
    protected native void OnControllerCallback(long nativeHandle, int what, long arg1, long arg2, Bundle bundle);

    public interface ControllerManagerEventListener {
        void OnControllerEvent(int what, long arg1, long arg2, Bundle bundle);
    }

    /**
     * App Context
     */
    WeakReference<Context> context;

    /**
     * Controller Event Listener
     */
    ControllerManagerEventListener controllerManagerEventListener;

    /**
     * native Object's Handle
     */
    long nativeHandle;

    /**
     * List of Controllers
     */
    IndexedList<ControllerContext> listOfControllers = new IndexedList<>(8);

    /**
     *
     * @param ctxt
     */
    SvrServiceClient svrServiceClient;

    /**
     * Handler for receiving callbacks from controllers
     */
    ControllerResponseHandler controllerResponseHandler = null;

    /**
     *
     */
    Intent defaultControllerIntent = null;

    /**
     *
     */
    int    defaultControllerRingBufferSize = 80;

    //-------------------------------------------------------------------------
    protected ControllerManager(Context ctxt, SvrServiceClient svrServiceClient)
    {
        this(ctxt, svrServiceClient, null, 0);
    }

    //-------------------------------------------------------------------------
    public ControllerManager(Context ctxt, SvrServiceClient svrServiceClient, long nativeHandle)
    {
        this(ctxt, svrServiceClient, null, nativeHandle);
    }
    /**
     *
     */
    //-------------------------------------------------------------------------
    public ControllerManager(Context ctxt, SvrServiceClient svrServiceClient, ControllerManagerEventListener callback )
    {
        this(ctxt, svrServiceClient, callback, 0);
    }

    //-------------------------------------------------------------------------
    private ControllerManager(Context ctxt, SvrServiceClient svrServiceClient, ControllerManagerEventListener eventListener, long nativeHandle)
    {
        this.controllerResponseHandler = new ControllerResponseHandler(this);
        this.svrServiceClient = svrServiceClient;
        this.context = new WeakReference<>(ctxt);
        this.controllerManagerEventListener = eventListener;
        this.nativeHandle = nativeHandle;
    }

    //-------------------------------------------------------------------------
    public void Initialize(String defaultService, int size)
    {
        Log.d(TAG, "Initialize(" + defaultService + ", " + size +")");

        if( defaultService != null ) {
            String[] parts = defaultService.split("/");
            if (parts.length == 2) {
                defaultControllerIntent = new Intent();
                defaultControllerIntent.setPackage(parts[0]);
                defaultControllerIntent.setClassName(parts[0], parts[1]);
            }
        }
        defaultControllerRingBufferSize = size;
    }

    //-------------------------------------------------------------------------
    public int Start(final String desc)
    {
        return Start(desc, -1, 0);
    }

    //-------------------------------------------------------------------------
    public int Start(final String desc, final int qvrfd, final int qvrfdSize)
    {
        int handle = -1;
        {
            final ControllerContext controllerContext = ControllerContext.CreateFor(desc, qvrfd, qvrfdSize, controllerResponseHandler);
            handle = controllerContext.handle = listOfControllers.Add(controllerContext);

            if( handle >= 0 ) {
                svrServiceClient.ExecuteOnConnection(new Runnable() {
                    @Override
                    public void run() {

                        Intent intent = defaultControllerIntent;
                        int size = defaultControllerRingBufferSize;

                        try {
                                if (svrServiceClient.GetInterface() != null) {
                                    Intent selectedIntent = svrServiceClient.GetInterface().GetControllerProviderIntent(desc);
                                    if( selectedIntent != null )
                                    {
                                        intent = selectedIntent;
                                    }
                                    size = svrServiceClient.GetInterface().GetControllerRingBufferSize();
                                }
                        } catch (RemoteException e) {
                                e.printStackTrace();
                        }
                        if( false == controllerContext.Start(context, intent, size) )
                        {
                            //post error
                            if( controllerResponseHandler != null ) {
                                Message msg = Message.obtain(null, SvrControllerApi.CONTROLLER_ERROR, controllerContext.handle, 0);
                                controllerResponseHandler.sendMessage(msg);
                            }
                        }
                        }
                });
            }
        }

        return handle;
    }

    //-------------------------------------------------------------------------
    public void Shutdown()
    {
        int size = listOfControllers.Size();
        for(int i=0;i<size;i++)
        {
            Stop(i);
        }
        nativeHandle = 0;
    }

    /**
     *
     * @param handle
     * @param what
     * @param arg1
     * @param arg2
     */
    //-------------------------------------------------------------------------
    public void SendMessage(int handle, int what, int arg1, int arg2)
    {
        {
            ControllerContext controllerContext = listOfControllers.Get(handle);
            if( controllerContext != null ) {
                controllerContext.SendMessage(what, arg1, arg2);
            }
        }
    }

    /**
     * ControllerStop
     */
    //-------------------------------------------------------------------------
    public void Stop(int handle)
    {
        {
            ControllerContext controllerContext = listOfControllers.Remove(handle);
            if( controllerContext != null ) {
                controllerContext.Stop();
            }
        }
    }

    /**
     *
     * @param handle
     * @param what
     * @return
     */
    //-------------------------------------------------------------------------
    public ByteBuffer Query(int handle, int what)
    {
        ByteBuffer serializedData = null;
        ControllerContext controllerContext = listOfControllers.Get(handle);

        if( controllerContext != null ) {
            switch (what) {
                case SvrControllerApi.svrControllerQueryType_kControllerQueryBatterRemaining:
                {
                    int data = controllerContext.QueryInt(what);
                    serializedData = ByteBuffer.allocateDirect(4);
                    serializedData.order(ByteOrder.LITTLE_ENDIAN);
                    serializedData.position(0);
                    serializedData.putInt(data);
                }
                break;
                case SvrControllerApi.svrControllerQueryType_kControllerQueryControllerCaps:
                {
                    
                    serializedData = ByteBuffer.allocateDirect(148);
                    int data = controllerContext.QueryInt(what);
                    serializedData.order(ByteOrder.LITTLE_ENDIAN);
                    serializedData.position(128);
                    serializedData.putInt(data);
                    data = controllerContext.QueryInt(SvrControllerApi.svrControllerQueryType_kControllerQueryActiveButtons);
                    serializedData.order(ByteOrder.LITTLE_ENDIAN);
                    serializedData.position(132);
                    serializedData.putInt(data);
                    data = controllerContext.QueryInt(SvrControllerApi.svrControllerQueryType_kControllerQueryActive2DAnalogs);
                    serializedData.order(ByteOrder.LITTLE_ENDIAN);
                    serializedData.position(136);
                    serializedData.putInt(data);
                    data = controllerContext.QueryInt(SvrControllerApi.svrControllerQueryType_kControllerQueryActive1DAnalogs);
                    serializedData.order(ByteOrder.LITTLE_ENDIAN);
                    serializedData.position(140);
                    serializedData.putInt(data);
                    data = controllerContext.QueryInt(SvrControllerApi.svrControllerQueryType_kControllerQueryActiveTouchButtons);
                    serializedData.order(ByteOrder.LITTLE_ENDIAN);
                    serializedData.position(144);
                    serializedData.putInt(data);
                    String fromService = controllerContext.QueryString(SvrControllerApi.svrControllerQueryType_kControllerQueryDeviceManufacturer);
                    serializedData.order(ByteOrder.LITTLE_ENDIAN);
                    serializedData.position(0);
                    byte[] descsBytes = fromService.getBytes();
                    serializedData.put(descsBytes);
                    fromService = controllerContext.QueryString(SvrControllerApi.svrControllerQueryType_kControllerQueryDeviceIdentifier);
                    serializedData.order(ByteOrder.LITTLE_ENDIAN);
                    serializedData.position(64);
                    byte[] deviceIdBytes = fromService.getBytes();
                    serializedData.put(deviceIdBytes);
                    
                }
                break;
            }
        }

        return serializedData;
    }

    private static final String TAG = "ControllerMgr";
    protected static final int CONTROLLER_MEMORY_ALLOCATED = 0x699;
}
