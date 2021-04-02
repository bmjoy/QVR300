package com.qti.acg.apps.controllers.ximmerse;

import android.os.ParcelFileDescriptor;
import android.util.Log;

import com.ximmerse.input.ControllerInput;
import com.ximmerse.sdk.XDeviceApi;
import com.ximmerse.sdk.XDeviceConstants;

public class ControllerContext {

    private static final String TAG = "XimmerseModule";

    static {
            System.loadLibrary("sharedmem");
    }

    private static native void updateNativeState(int ptr, int state, int btns,
                                                            float pos0, float pos1, float pos2,
                                                            float rot0, float rot1, float rot2, float rot3,
                                                            float gyro0, float gyro1, float gyro2,
                                                            float acc0, float acc1, float acc2,
                                                            int timestamp,
                                                            int touchpads, float x, float y);

    private static native void updateNativeConnectionState(int ptr, int connectionState);

    private static native int createNativeContext(int fd, int size);

    int id = -1;
    int deviceId = -1;
    ControllerInput deviceInput = null;
    XimmerseControllerService.DeviceInfo di = null;

    int fd = -1;
    int fdSize = 0;
    int nativeContextPtr = 0;

    private ControllerInput.State state = null;

    Thread t = null;
    long lastUpdateTime = 0;
    boolean bCheckForUpdates = true;

    /**
     *
     * @return
     */
    //----------------------------------------------------------------------------------------------
    public static ControllerContext CreateFor(int handle, String desc, ParcelFileDescriptor pfd, int fdSize, XimmerseControllerService.DeviceInfo deviceInfo)
    {
        ControllerContext controllerContext = new ControllerContext();
        controllerContext.id= handle;
        controllerContext.fd = pfd.getFd();
        controllerContext.fdSize = fdSize;
        controllerContext.di= deviceInfo;
        return controllerContext;
    }

    /**
     *
     */
    //----------------------------------------------------------------------------------------------
    private void updateNativeState()
    {
        deviceInput.updateState();
        state = deviceInput.getState(state);

        int Trigger = ((state.buttons & ControllerInput.BUTTON_LEFT_TRIGGER) != 0) ? 1 : 0;
        int ThumbClick = ((state.buttons & ControllerInput.BUTTON_LEFT_THUMB) != 0) ? 1 : 0;
        int Back = ((state.buttons & ControllerInput.BUTTON_BACK) != 0) ? 1 : 0;
        int Start = ((state.buttons & ControllerInput.BUTTON_START) != 0) ? 1 : 0;
        int DpadUp = ((state.buttons & ControllerInput.BUTTON_DPAD_UP) != 0) ? 1 : 0;
        int DpadRight = ((state.buttons & ControllerInput.BUTTON_DPAD_RIGHT) != 0) ? 1 : 0;
        int DpadDown = ((state.buttons & ControllerInput.BUTTON_DPAD_DOWN) != 0) ? 1 : 0;
        int DpadLeft = ((state.buttons & ControllerInput.BUTTON_DPAD_LEFT) != 0) ? 1 : 0;
        int Touchpad = ((state.buttons & ControllerInput.BUTTON_LEFT_THUMB_MOVE) !=0) ? 1 :0;

        int buttons = (Trigger << 13) |
                (ThumbClick<<15) |
                (DpadUp << 16 ) |
                (DpadRight << 19) |
                (DpadDown << 17) |
                (DpadLeft << 18) |
                (Back <<9) |
                (Start << 8);

        int touchpads = (Touchpad << 4);

        updateNativeState(nativeContextPtr,
                2,
                buttons,
                state.position[0], state.position[1], state.position[2],
                state.rotation[0], state.rotation[1], state.rotation[2], state.rotation[3],
                state.gyroscope[0], state.gyroscope[1],state.gyroscope[2],
                state.accelerometer[0], state.accelerometer[1], state.accelerometer[2],
                state.timestamp,
                touchpads, state.axes[2], state.axes[3]);

        lastUpdateTime = System.currentTimeMillis();
    }

    //----------------------------------------------------------------------------------------------
    Runnable stateListener = new Runnable() {
        @Override
        public void run() {
            updateNativeState();
        }
    };

    //----------------------------------------------------------------------------------------------
    Runnable checkForUpdates = new Runnable() {
        @Override
        public void run() {
            while(bCheckForUpdates) {
                if( System.currentTimeMillis() - lastUpdateTime > 1000 )
                {
                    updateNativeConnectionState(nativeContextPtr, 3);
                }
                try {
                    Thread.sleep(1000);
                } catch (InterruptedException e) {
                    e.printStackTrace();
                }
                //Log.e("X3", "check Updates Thread");
            }
        }
    };

    //
    //
    //
    //----------------------------------------------------------------------------------------------
    public boolean Start()
    {
        Log.e(TAG, "Starting " + di.identifier);
        boolean bResult = false;
        nativeContextPtr = createNativeContext(fd, fdSize);

        if( nativeContextPtr != 0 ) {
            deviceId = XDeviceApi.getInputDeviceHandle(di.identifier);
            Log.e(TAG, " Device ID = " + deviceId);
            if( deviceId >= 0 ) {
                deviceInput = new ControllerInput();//deviceId, di.identifier);//
                state = deviceInput.getState(null);
                deviceInput.handle = deviceId;
                deviceInput.name = di.identifier;
                updateNativeConnectionState(nativeContextPtr, 3);
                XDeviceApi.addOnUpdateStateListener(deviceId, stateListener);

                t = new Thread(checkForUpdates);
                bCheckForUpdates = true;
                t.start();
                bResult = true;
            }
        }

        return bResult;
    }

    //
    //
    //----------------------------------------------------------------------------------------------
    public void Stop()
    {
        bCheckForUpdates = false;
        t = null;
        XDeviceApi.removeOnUpdateStateListener(deviceId, stateListener);
    }

    //
    //
    //----------------------------------------------------------------------------------------------
    public void reset() {
        XDeviceApi.sendMessage(deviceId, XDeviceConstants.kMessage_RecenterSensor, 0, 0);
    }

    //
    //
    //----------------------------------------------------------------------------------------------
    public void sendEvent(int what, int arg1, int arg2)
    {
        Log.e(TAG, "Send Event " + what + " -- " + arg1 + " --- " + arg2);
        switch( what )
        {
            case 1:
                Log.e(TAG, " Trigger Vibration ");
                int strength = 20; //(20~100) %
                int frequency = 0; //(default 0)
                int duration = 500; //ms
                XDeviceApi.sendMessage(deviceId, XDeviceConstants.kMessage_TriggerVibration,
                        (int) ((strength <= 0 ? 20 : strength) | ((frequency << 16) & 0xFFFF0000)),
                        (int) (duration * 1000));
                break;
            case 201:
                XDeviceApi.sendMessage(deviceId, XDeviceConstants.kMessage_TriggerVibration, -1, 0);
                break;
        }
    }
}
