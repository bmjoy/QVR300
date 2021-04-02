//-----------------------------------------------------------------------------
//  Copyright (c) 2017 Qualcomm Technologies, Inc.
//  All Rights Reserved. Qualcomm Technologies Proprietary and Confidential.
//-----------------------------------------------------------------------------

package com.qualcomm.svrapi.controllers;

import android.content.Context;
import android.content.Intent;
import android.os.Handler;
import android.os.Message;
import android.os.ParcelFileDescriptor;
import android.os.RemoteException;
//import android.util.Log;
import java.io.IOException;
import java.lang.ref.WeakReference;

/**
 *
 */
class ControllerContext implements ControllerConnection.OnControllerConnectionListener {

    private IControllerInterfaceCallback mCallback = new IControllerInterfaceCallback.Stub() {
        @Override
        public void onStateChanged(int handle, int what, int arg1, int arg2) throws RemoteException {
            if( responseMessenger != null )
            {
                responseMessenger.sendMessage(Message.obtain(null, what, arg1, arg2));
            }
        }
    };
    private int qvrFdSize;
    private int qvrfd;

    private static native long  AllocateSharedMemory(int sz );
    private static native int  GetFileDescriptor(long ptr);
    private static native int  GetFileDescriptorSize(long ptr);
    private static native void FreeSharedMemory(long fd);

    /**
     *
     */
    private ControllerContext() {}

    /**
     *
     */
    private String controllerDesc ="";
    /**
     * api to talk to the controller
     */
    IControllerInterface controllerApi = null;
    /**
     * handle in the client
     */
    int handle = -1;

    /**
     * fileDescriptor for sharedMemory
     */
    ParcelFileDescriptor pfd = null;
    /**
     *
     */
    int fdSize = 0;
    /**
     *
     */
    long nativeRingBufferPtr = 0;

    /**
     *
     */
    Handler responseMessenger = null;

    /**
     *
     */
    int ringBufferSize = 0;

    /**
     *
     */
    private void free( )
    {
        try {
            if( pfd != null ) {
                pfd.close();
            }
            if( nativeRingBufferPtr != 0) {
                FreeSharedMemory(nativeRingBufferPtr);
            }
            pfd = null;
            nativeRingBufferPtr = 0;
        } catch (IOException e) {
            e.printStackTrace();
        }
    }

    /**
     *
     * @param context
     */
    public boolean Start(final WeakReference<Context> context, Intent intent, int size)
    {
        ringBufferSize = size;

        boolean result = ControllerConnectionManager.RequestConnection(intent, context, this);

        return result;
    }

    public void SendMessage(int what, int arg1, int arg2)
    {
        if( controllerApi != null )
        {
            try {
                controllerApi.SendMessage(handle, what, arg1, arg2);
            } catch (RemoteException e) {
                e.printStackTrace();
            }
        }
    }

    /**
     *
     */
    @Override
    public void OnApiAvailable(Intent intent, IControllerInterface controllerInterface) {

        try {
            controllerApi = controllerInterface;
            controllerApi.registerCallback(mCallback);
            nativeRingBufferPtr = AllocateSharedMemory(ringBufferSize);
            int fd = GetFileDescriptor(nativeRingBufferPtr);
            pfd = ParcelFileDescriptor.adoptFd(fd);
            fdSize = GetFileDescriptorSize(nativeRingBufferPtr);

            ParcelFileDescriptor qvrFd = null;
            try {
                 qvrFd = ParcelFileDescriptor.fromFd(qvrfd);
            } catch (IOException e) {
                e.printStackTrace();
            }
            controllerApi.Start(handle, controllerDesc, pfd, fdSize, qvrFd, qvrFdSize);
            //TODO: CacheNativePtr
            Message msg = Message.obtain(null, ControllerManager.CONTROLLER_MEMORY_ALLOCATED, handle, 0);
            responseMessenger.sendMessage(msg);
        } catch (RemoteException e) {
            e.printStackTrace();
        }

    }

    /**
     *
     */
    public void Stop()
    {
        if (controllerApi != null) {
            if (controllerApi != null) {
                try {
                    controllerApi.Stop(handle);
                } catch (RemoteException e) {
                    e.printStackTrace();
                }
            }
            controllerApi = null;
        }

        free();
    }

    /**
     *
     */
    @Override
    public void OnApiLost(Intent intent) {
        controllerApi = null;
        responseMessenger.sendMessage(Message.obtain(null, SvrControllerApi.CONTROLLER_ERROR, handle, 0, 0));
    }

    /**
     *
     * @param desc
     * @return
     */
    public static ControllerContext CreateFor(String desc, int qvrfd, int qvrfdSize, Handler handler) {
        ControllerContext controllerContext = new ControllerContext();
        controllerContext.controllerDesc = desc;
        controllerContext.responseMessenger = handler;
        controllerContext.qvrfd = qvrfd;
        controllerContext.qvrFdSize = qvrfdSize;
        return controllerContext;
    }

    /**
     *
     * @param what
     * @return
     */
    public int QueryInt(int what)
    {
        int result = -1;
        try {
            if( controllerApi != null )
            {
                result = controllerApi.QueryInt(handle, what);
            }
        } catch (RemoteException e) {
            e.printStackTrace();
        }
        return result;
    }

    /**
     *
     * @param what
     * @return
     */
    public String QueryString(int what)
    {
        String result = null;
        try {
            if( controllerApi != null )
            {
                result = controllerApi.QueryString(handle, what);
            }
        } catch (RemoteException e) {
            e.printStackTrace();
        }
        return result;
    }

    private static final String TAG = "ControllerContext";
}
