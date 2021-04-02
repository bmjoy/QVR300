package com.qti.acg.apps.controllers.ximmerse;

import android.os.Message;
import android.os.ParcelFileDescriptor;
import android.os.Process;
import android.os.RemoteException;
import android.util.Log;

import com.qualcomm.svrapi.controllers.SvrControllerApi;
import com.ximmerse.sdk.XDeviceApi;
import com.ximmerse.sdk.XDeviceConstants;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;

public class XimmerseControllerService extends SvrControllerModule {

    private static final String TAG = "XimmerseModule";
    HashMap<Integer, ControllerContext> listOfControllers = new HashMap<>(2);
    List<DeviceInfo> listOfDevices = new ArrayList<>();
    boolean bInitialized = false;

    ////////////////////////////////////////////////////////////////////////////////////////////////
    class DeviceInfo {
        String identifier;
        boolean isInUse;

        DeviceInfo(String s)
        {
            identifier = s;
            isInUse = false;
        }

        void MarkInUse()
        {
            isInUse = true;
        }
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////
    void updateDeviceList()
    {
        listOfDevices.add(new DeviceInfo("XCobra-0"));
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////
    @Override
    public void onCreate() {
        super.onCreate();

        updateDeviceList();
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////
    @Override
    public void ControllerStop(int id)
    {
        Log.e(TAG, "ControllerStop");
        ControllerContext controllerContext = listOfControllers.get(id);
        if( controllerContext != null ) {
            listOfControllers.remove(id);
            controllerContext.Stop();
            controllerContext.di.isInUse = false;
        }
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////
    DeviceInfo GetAvailableDevice()
    {
        DeviceInfo di = null;
        for(int i=0;i<listOfDevices.size();i++)
        {
            if( listOfDevices.get(i).isInUse == false )
            {
                di = listOfDevices.get(i);
                break;
            }
        }
        return di;
    }

    /**
     * Initialize
     */
    ////////////////////////////////////////////////////////////////////////////////////////////////
    void initializeIfNeeded()
    {
        //if( bInitialized == false )
        {
            Log.e(TAG, "XDeviceApi::init");
            XDeviceApi.init(this);
            bInitialized = true;
        }
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////
    ControllerContext GetAvailableController(int handle, String desc, ParcelFileDescriptor pfd, int fdSize)
    {
        ControllerContext controllerContext = null;

        DeviceInfo deviceInfo = GetAvailableDevice();
        if( deviceInfo != null ) {
            controllerContext = ControllerContext.CreateFor(handle, desc, pfd, fdSize, deviceInfo);
            if( controllerContext != null )
            {
                deviceInfo.MarkInUse();
                listOfControllers.put(controllerContext.id, controllerContext);
            }
        }

        return controllerContext;
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////
    @Override
    protected void ControllerStart(int handle, String desc, ParcelFileDescriptor pfd, int fdSize) {
        Log.e(TAG, "ControllerStart");
        initializeIfNeeded();

        ControllerContext controllerContext = GetAvailableController(handle, desc, pfd, fdSize);

        if(controllerContext != null)
        {
            if( true == controllerContext.Start() ) {
                OnDeviceStateChanged(handle, SvrControllerApi.CONTROLLER_CONNECTING);
            }
            else {
                OnDeviceStateChanged(handle, SvrControllerApi.CONTROLLER_ERROR);
            }
        }
        else {
            //TODO: Not Available
        }
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////
    protected void ControllerExit()
    {
        if( bInitialized )
        {
            XDeviceApi.exit();
            bInitialized = false;
            stopSelf();
            Process.killProcess(Process.myPid());
        }
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////
    @Override
    protected void ControllerStopAll() {
        int size = listOfControllers.size();
        for(int i=0;i<size;i++)
        {
            ControllerStop(i);
        }
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////
    protected int ControllerQueryInt(int handle, int what)
    {
        int result = -1;
        ControllerContext controllerContext = listOfControllers.get(handle);
        if( controllerContext != null )
        {
            switch(what)
            {
                case SvrControllerApi.svrControllerQueryType_kControllerQueryBatterRemaining:
                    result = XDeviceApi.getInt(controllerContext.deviceId, XDeviceConstants.kField_BatteryLevel, 0);
                    break;
                case SvrControllerApi.svrControllerQueryType_kControllerQueryControllerCaps:
                    //Log.e(TAG, "ControllerStart Query Capability");
                    result = 0;
                    break;
                case SvrControllerApi.svrControllerQueryType_kControllerQueryActiveButtons:
                    //Log.e(TAG, "ControllerStart Query ActiveButtons");
                    result = 4;
                    break;
                case SvrControllerApi.svrControllerQueryType_kControllerQueryActive2DAnalogs:
                    //Log.e(TAG, "ControllerStart Query Active2DAnalogs");
                    result = 1;
                    break;
                case SvrControllerApi.svrControllerQueryType_kControllerQueryActive1DAnalogs:
                    //Log.e(TAG, "ControllerStart Query Active1DAnalogs");
                    result = 0;
                    break;
                case SvrControllerApi.svrControllerQueryType_kControllerQueryActiveTouchButtons:
                    //Log.e(TAG, "ControllerStart Query ActiveTouchButtons");
                    result = 1;
                    break;
            }
        }
        return result;
    }
    protected String ControllerQueryString(int handle, int what)
    {
        String result = null;
        ControllerContext controllerContext = listOfControllers.get(handle);
        if( controllerContext != null )
        {
            switch(what)
            {
                case SvrControllerApi.svrControllerQueryType_kControllerQueryDeviceManufacturer:
                    //Log.e(TAG, "ControllerStart Query DeviceManufacturer");
                    result = "Ximmerse Controller";
                    break;
                case SvrControllerApi.svrControllerQueryType_kControllerQueryDeviceIdentifier:
                    //Log.e(TAG, "ControllerStart Query DeviceIdentifier");
                    result = "12345678";
                    break;
            }

        }
        return result;
    }
    ////////////////////////////////////////////////////////////////////////////////////////////////
    @Override
    protected void ControllerRecenter(Message msg) {
        ControllerContext controllerContext = listOfControllers.get(msg.arg1);
        if( controllerContext != null ) {
            controllerContext.reset();
        }
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////
    @Override
    protected void ControllerSendMessage(int handle, int what, int arg1, int arg2) {
        //TODO:
        ControllerContext controllerContext = listOfControllers.get(handle);
        if( controllerContext != null )
        {
            switch(what)
            {
                case SvrControllerApi.svrControllerMessageType_kControllerMessageRecenter:
                    controllerContext.reset();
                    break;
            }
        }
    }


    ////////////////////////////////////////////////////////////////////////////////////////////////
    public void OnDeviceStateChanged(int deviceId, int state) {
        Log.e(TAG, "OnDeviceStateChanged (" + state +", " + deviceId + ")" );

        ControllerContext cc = listOfControllers.get(deviceId);
        try {
            callback.onStateChanged(cc.id, state, 0, 0);
        } catch (RemoteException e) {
            e.printStackTrace();
        }
    }
}
