package com.serenegiant.helper;

import android.content.Context;
import android.hardware.usb.UsbDevice;
import android.hardware.usb.UsbDeviceConnection;
import android.hardware.usb.UsbEndpoint;
import android.hardware.usb.UsbInterface;
import android.os.Build;
import android.os.Handler;
import android.os.SystemClock;
import android.support.annotation.RequiresApi;
import android.text.TextUtils;
import android.util.Log;
import android.widget.Toast;

import com.serenegiant.usb.USBMonitor;

import java.io.ByteArrayInputStream;
import java.io.DataInputStream;
import java.io.IOException;

public class USBHelper {

    private static final String TAG = "USBHelper";
    private USBMonitor mUSBMonitor;
    private Context context;
    private UsbDeviceConnection usbDeviceConnection;


    private boolean mExit = false;



    private final USBMonitor.OnDeviceConnectListener mOnDeviceConnectListener = new USBMonitor.OnDeviceConnectListener() {

        @Override
        public void onAttach(final UsbDevice device) {

            if (device.getVendorId() == 1449 || device.getVendorId() == 0x05A9) {

                new Handler(context.getMainLooper()).post(new Runnable() {
                    @RequiresApi(api = Build.VERSION_CODES.LOLLIPOP)
                    @Override
                    public void run() {
                        Toast.makeText(context, "onAttach " + device.getProductName(), Toast.LENGTH_SHORT).show();
                    }
                });

                mUSBMonitor.requestPermission(device);
            }
        }

        @Override
        public void onDettach(UsbDevice device) {

        }

        @Override
        public void onConnect(final UsbDevice device, USBMonitor.UsbControlBlock ctrlBlock, boolean createNew) {
            if (device.getVendorId() == 1449 || device.getVendorId() == 0x05A9 && usbDeviceConnection == null) {

//            if (device.getVendorId() == 1158) {
                new Handler(context.getMainLooper()).post(new Runnable() {
                    @RequiresApi(api = Build.VERSION_CODES.LOLLIPOP)
                    @Override
                    public void run() {
                        Toast.makeText(context, "onConnect " + device.getProductName(), Toast.LENGTH_SHORT).show();
                    }
                });

                usbDeviceConnection = ctrlBlock.getConnection();
                openCamera(usbDeviceConnection.getFileDescriptor(), device);
            }
        }

        @Override
        public void onDisconnect(UsbDevice device, USBMonitor.UsbControlBlock ctrlBlock) {
            if (device.getVendorId() == 1449 || device.getVendorId() == 0x05A9) {
                mExit = true;
            }
        }

        @Override
        public void onCancel(UsbDevice device) {

        }
    };


    public USBHelper() {

    }

    public void init(Context context) {
        if (mUSBMonitor == null)
            mUSBMonitor = new USBMonitor(context, mOnDeviceConnectListener);
        mUSBMonitor.register();

        this.context = context;
    }

    public void destroy() {

        mExit = true;

        if (mUSBMonitor != null)
            mUSBMonitor.unregister();

        if (usbDeviceConnection != null) {
            usbDeviceConnection.close();
            usbDeviceConnection = null;
        }

        Native.closeUVCCamera();
    }

    private void openCamera(final int fd, final UsbDevice device) {
        final String name = device.getDeviceName();
        String[] v = !TextUtils.isEmpty(name) ? name.split("/") : null;
        final int busnum = Integer.parseInt(v[v.length - 2]);;
        final int devnum = Integer.parseInt(v[v.length - 1]);;
//        if (v != null) {
//            busnum =
//            devnum =
//        }


        Log.i(TAG, "run: vid = " + device.getVendorId());
        Log.i(TAG, "run: pid = " + device.getProductId());
        Log.i(TAG, "run: fd = " + fd);
        Log.i(TAG, "run: busnum = " + busnum);
        Log.i(TAG, "run: devnum = " + devnum);
        Log.i(TAG, "run: usbfs = " + getUSBFSName(name));
        try {
            Native.openUVCCamera(device.getVendorId(), device.getProductId(), fd, busnum, devnum, getUSBFSName(name));

            new Thread(new Runnable() {
                @Override
                public void run() {

                    SystemClock.sleep(2000);

                    Native.openHid(device.getVendorId(), device.getProductId(), fd, busnum, devnum, getUSBFSName(name));

                    while (!mExit) {
                        Native.readHid();
                    }
                }
            }).start();


        } catch (IllegalStateException e) {
            Log.e(TAG, "IllegalStateException open camera");
        }
    }

    private String getUSBFSName(String name) {
        String result = null;
        final String[] v = !TextUtils.isEmpty(name) ? name.split("/") : null;
        if ((v != null) && (v.length > 2)) {
            final StringBuilder sb = new StringBuilder(v[0]);
            for (int i = 1; i < v.length - 2; i++)
                sb.append("/").append(v[i]);
            result = sb.toString();
        }
        return result;
    }

}
