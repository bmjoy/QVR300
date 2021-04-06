package com.serenegiant.helper;

import android.content.res.AssetManager;

public class Native {

    static {
        System.loadLibrary("uvc_native");
    }

    public static native String stringFromJNI();

    public static native int openUVCCamera(int venderId, int productId, int fileDescriptor, int busNum, int devAddr, String usbfs);

    public static native void closeUVCCamera();

    public static native int openHid(int venderId, int productId, int fileDescriptor, int busNum, int devAddr, String usbfs);

    public static native void readHid();

    public static native void renderInit();

    public static native void renderUnInit();

    public static native void renderUpdateTransformMatrix(float rotateX, float rotateY, float scaleX, float scaleY);

    public static native void getPose(float[] array);

    public static native void renderOnSurfaceCreated();

    public static native void renderOnSurfaceChanged(int width, int height);

    public static native void renderOnDrawFrame();


//    public static native void setAssetManager(AssetManager assetManager, int target);

}
