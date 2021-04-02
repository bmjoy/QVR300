//=============================================================================
// FILE: SvrApi.java
//                  Copyright (c) 2016-2017 QUALCOMM Technologies Inc.
//                              All Rights Reserved.
//
//==============================================================================

package com.qualcomm.svrapi;

import android.app.Activity;
import android.app.AlertDialog;
import android.content.DialogInterface;
import android.graphics.Point;
import android.content.ActivityNotFoundException;
import android.view.View;
import android.view.Display;
import android.view.WindowManager;
import android.view.Choreographer;
import android.view.Surface;
import android.util.Log;
import android.opengl.Matrix;

public class SvrApi implements android.view.Choreographer.FrameCallback 
{
    public static final String TAG = "SvrApi";
    public static SvrApi handler = new SvrApi();

    static 
	{
        System.loadLibrary("svrapi");
    }

    public static native void nativeVsync(long lastVsyncNano);

    public static native void svrInitialize(Activity javaActivity);

    public static native void svrBeginVr(Activity javaActivity, svrBeginParams mBeginParams);

    public static native void svrSubmitFrame(Activity javaActivity, svrFrameParams mSvrFrameParams);

    public static native void svrEndVr();

    public static native void svrShutdown();

    public static native String svrGetVersion();

    public static native String svrGetVrServiceVersion();

    public static native String svrGetVrClientVersion();

    public static native float svrGetPredictedDisplayTime();

    public static native float svrGetPredictedDisplayTimePipelined(int depth);

    public static native svrHeadPoseState svrGetPredictedHeadPose(float predictedTimeMs);

    public static native void svrSetPerformanceLevels(svrPerfLevel cpuPerfLevel, svrPerfLevel gpuPerfLevel);

    public static native svrDeviceInfo svrGetDeviceInfo();

    public static native void svrRecenterPose();

    public static native void svrRecenterPosition();

    public static native void svrRecenterOrientation(boolean yawOnly);

    public static native int svrGetSupportedTrackingModes();

    public static native void svrSetTrackingMode(int trackingModes);

    public static native int svrGetTrackingMode();

    public static native void svrBeginEye(svrWhichEye whichEye);

    public static native void svrEndEye(svrWhichEye whichEye);

    public static native void svrSetWarpMesh(svrWarpMeshEnum whichMesh, float[] pVertexData, int vertexSize, int nVertices, int[] pIndices, int nIndices);

    public static native void svrGetOcclusionMesh(svrWhichEye whichEye, int[] pTriangleCount, int[] pVertexStride, float[] pTriangles);

    public static Choreographer choreographerInstance;

    public static void NotifyNoVr( final Activity act )
    {
        act.runOnUiThread(new Runnable() {
                public void run() {

                    try
                    {
                    AlertDialog.Builder alertBuilder = new AlertDialog.Builder(act);
                    alertBuilder.setMessage("SnapdragonVR not supported on this device!");
                    alertBuilder.setCancelable(true);

                    alertBuilder.setPositiveButton(
                        "Close",
                        new DialogInterface.OnClickListener() {
                            public void onClick(DialogInterface dialog, int id) {
                                dialog.cancel();
                            }
                        });

                    AlertDialog alertDlg = alertBuilder.create();
                    alertDlg.show();
                    }
                    catch (Exception e)
                    {
                        Log.e(TAG, "Exception displaying dialog box!");
                        Log.e(TAG, e.getMessage());
                    }
            }
        });
    }

    public static long getVsyncOffsetNanos( Activity act)
    {
         final Display disp = act.getWindowManager().getDefaultDisplay();
		 return disp.getAppVsyncOffsetNanos();
    }
	
	public static float getRefreshRate( Activity act )
	{
		final Display disp = act.getWindowManager().getDefaultDisplay();
		return disp.getRefreshRate();
	}
	
	public static int getDisplayWidth( Activity act )
	{
		final Display disp = act.getWindowManager().getDefaultDisplay();
		Point outSize = new Point();
		disp.getRealSize(outSize);
		return outSize.x;
	}
	
	public static int getDisplayHeight( Activity act )
	{
		final Display disp = act.getWindowManager().getDefaultDisplay();
		Point outSize = new Point();
		disp.getRealSize(outSize);
		return outSize.y;
	}
	
	public static int getDisplayOrientation( Activity act )
	{
		final Display disp = act.getWindowManager().getDefaultDisplay();
		int rot = disp.getRotation();
		if(rot == Surface.ROTATION_0)
		{
			return 0;
		}
		else if(rot == Surface.ROTATION_90)
		{
			return 90;
		}
		else if(rot == Surface.ROTATION_180)
		{
			return 180;
		}
		else if(rot == Surface.ROTATION_270)
		{
			return 270;
		}
		else
		{
			return -1;
		}
	}
	
	public static int getAndroidOsVersion( )
	{
		return android.os.Build.VERSION.SDK_INT;
	}

    public static void startVsync( Activity act )
    {
    	act.runOnUiThread( new Thread()
    	{
		 @Override
    		public void run()
    		{
				choreographerInstance = Choreographer.getInstance();
				choreographerInstance.removeFrameCallback(handler);
				choreographerInstance.postFrameCallback(handler);
    		}
    	});
	}

    public static void stopVsync( Activity act )
    {
    	act.runOnUiThread( new Thread()
    	{
		 @Override
    		public void run()
    		{
				if (choreographerInstance != null) {
					choreographerInstance.removeFrameCallback(handler);
				}
    		}
    	});
	}

    public void doFrame(long frameTimeNanos)
    {
		nativeVsync(frameTimeNanos);
		choreographerInstance.postFrameCallback(this);
	}

    private static final int SVR_MAX_RENDER_LAYERS = 16;
    private static final int SVR_MAJOR_VERSION = 2;
    private static final int SVR_MINOR_VERSION = 1;
    private static final int SVR_REVISION_VERSION = 1;
    public static SvrResult svrResult;
    public static svrWarpMeshType warpMeshType;

    public enum SvrResult
    {
        SVR_ERROR_NONE(0),
        SVR_ERROR_UNKNOWN(1),
        SVR_ERROR_UNSUPPORTED(2),
        SVR_ERROR_VRMODE_NOT_INITIALIZED(3),
        SVR_ERROR_VRMODE_NOT_STARTED(4),
        SVR_ERROR_VRMODE_NOT_STOPPED(5),
        SVR_ERROR_QVR_SERVICE_UNAVAILABLE(6),
        SVR_ERROR_JAVA_ERROR(7);

        private int vResult;
        SvrResult(int value) { this.vResult = value; }

    }
   //enum permits only private constructors
    public void setSvrResult(int enumIndex)
    {
        switch (enumIndex)
        {
            case 0:
                svrResult = SvrResult.SVR_ERROR_NONE;
                break;
            case 1:
                svrResult = SvrResult.SVR_ERROR_UNKNOWN;
                break;
            case 2:
                svrResult = SvrResult.SVR_ERROR_UNSUPPORTED;
                break;
            case 3:
                svrResult = SvrResult.SVR_ERROR_VRMODE_NOT_INITIALIZED;
                break;
            case 4:
                svrResult = SvrResult.SVR_ERROR_VRMODE_NOT_STARTED;
                break;
            case 5:
                svrResult = SvrResult.SVR_ERROR_VRMODE_NOT_STOPPED;
                break;
            case 6:
                svrResult = SvrResult.SVR_ERROR_QVR_SERVICE_UNAVAILABLE;
                break;
            case 7:
                svrResult = SvrResult.SVR_ERROR_JAVA_ERROR;
                break;
        }
    }

    public void setSvrWarpMeshType(int enumIndex)
    {
        switch (enumIndex)
        {
            case 0:
                warpMeshType = svrWarpMeshType.kMeshTypeColumsLtoR;
                break;
            case 1:
                warpMeshType = svrWarpMeshType.kMeshTypeColumsRtoL;
                break;
            case 2:
                warpMeshType = svrWarpMeshType.kMeshTypeRowsTtoB;
                break;
            case 3:
                warpMeshType = svrWarpMeshType.kMeshTypeRowsBtoT;
                break;
        }
    }

    public enum svrEventType
    {
        kEventNone(0),
        kEventSdkServiceStarting(1),
        kEventSdkServiceStarted(2),
        kEventSdkServiceStopped(3),
        kEventControllerConnecting(4),
        kEventControllerConnected(5),
        kEventControllerDisconnected(6),
        kEventThermal(7),
        kEventSensorError(8);

        private int vSvrEventType;
        svrEventType(int value) { this.vSvrEventType = value; }

        @Override
        public String toString()
        {
            return String.valueOf(this.vSvrEventType);
        }
    }

    public class svrVector3
    {
        public float x, y, z;

        public svrVector3() { x = y = z = 0.f; }

        public svrVector3(float x, float y, float z)
        {
            this.x = x; this.y = y; this.z = z;
        }
    }

    public class svrVector4
    {
        public float x, y, z, w;

        public svrVector4()
        {
            x = 0.0f; y = 0.0f; z = 0.0f; w = 0.0f;
        }

        public svrVector4(float x, float y, float z, float w)
        {
            this.x = x; this.y = y; this.z = z; this.w = w;
        }

        public void set(float x, float y, float z, float w)
        {
            this.x = x; this.y = y; this.z = z; this.w = w;
        }

    }
    public class svrQuaternion
    {
        public float x, y, z, w;
        public float[] element = null;

        public svrQuaternion() { setIdentity(); }

        public void setIdentity()
        {
            x = y = z = 0;
            w = 1;
        }
        public svrMatrix4 quatToMatrix()
        {

            if (null == element) { element = new float[16]; }

            final float xx = x * x;
            final float xy = x * y;
            final float xz = x * z;
            final float xw = x * w;
            final float yy = y * y;
            final float yz = y * z;
            final float yw = y * w;
            final float zz = z * z;
            final float zw = z * w;

            element[0] = 1.0f - 2 * (yy + zz);
            element[1] = 2 * (xy + zw);
            element[2] = 2 * (xz - yw);
            element[3] = 0.0f;

            element[4] = 2 * (xy - zw);
            element[5] = 1 - 2 * (xx + zz);
            element[6] = 2 * (yz + xw);
            element[7] = 0.0f;

            element[8] = 2 * (xz + yw);
            element[9] = 2 * (yz - xw);
            element[10] = 1 - 2 * (xx + yy);
            element[11] = 0.0f;

            element[12] = 0.0f;
            element[13] = 0.0f;
            element[14] = 0.0f;
            element[15] = 1.0f;

            return new svrMatrix4(element);
        }

        public svrQuaternion set(float x, float y, float z, float w)
        {
            this.x = x; this.y = y; this.z = z; this.w = w;
            return this;
        }

        public boolean equals(final Object targetObj)
        {
            if (this == targetObj) return true;
            if (!(targetObj instanceof svrQuaternion)) return false;
            final svrQuaternion temp = (svrQuaternion) targetObj;
            return this.x == temp.x && this.y == temp.y && this.z == temp.z && this.w == temp.w;
        }
    }
    public class svrMatrix4
    {
        float mtx[];
        public svrMatrix4()
        {
            float[] mtx =
            {
                    0.0f, 0.0f, 0.0f, 0.0f,
                    0.0f, 0.0f, 0.0f, 0.0f,
                    0.0f, 0.0f, 0.0f, 0.0f,
                    0.0f, 0.0f, 0.0f, 0.0f
            };
        }
        public svrMatrix4(float[] m) { mtx = m; }
        public float[] queueInArray() { return mtx; }
    }
    public enum svrWhichEye
    {
        kLeftEye(0),
        kRightEye(1),
        kNumEyes(2);
        private int vWhichEye;

        svrWhichEye(int value) { this.vWhichEye = value; }
    }

    public enum svrEyeMask
    {
        kEyeMaskLeft(1),
        kEyeMaskRight(2),
        kEyeMaskBoth(3);
        private int vEyeMask;

        svrEyeMask(int value) { this.vEyeMask = value; }
        public int getEyeMask(){ return vEyeMask; }
    }

    public enum svrLayerFlags
    {
        kLayerFlagNone(0),
        kLayerFlagHeadLocked(1),
        kLayerFlagOpaque(2);
        private int vLayerFlags;

        svrLayerFlags(int value){ this.vLayerFlags = value; }
    }

    public enum svrColorSpace
    {
        kColorSpaceLinear(0),
        kColorSpaceSRGB(1),
        kNumColorSpaces(2);
        private int vColorSpace;

        svrColorSpace(int value){ this.vColorSpace = value; }
    }
    public class svrHeadPose
    {
        public svrQuaternion rotation;
        public svrVector3 position;

        public svrHeadPose()
        {
            rotation = new svrQuaternion();
            position = new svrVector3();
        }
    }

    public enum svrTrackingMode
    {
        kTrackingRotation(1),
        kTrackingPosition(2);

        private int vTrackingMode;
        svrTrackingMode(int value) { this.vTrackingMode = value; }
        public int getTrackingMode(){ return vTrackingMode; }
    }
    public class svrHeadPoseState
    {
        public svrHeadPose pose;
        public int poseStatus;
        public long poseTimeStampNs;
        public long poseFetchTimeNs;
        public long expectedDisplayTimeNs;

        public svrHeadPoseState()
        {
            pose = new svrHeadPose();
            poseStatus = 0;
            poseTimeStampNs = 0;
            poseFetchTimeNs = 0;
            expectedDisplayTimeNs = 0;
        }
    }

    public enum svrPerfLevel
    {
        kPerfSystem(0),
        kPerfMinimum(1),
        kPerfMedium(2),
        kPerfMaximum(3),
        kNumPerfLevels(4);

        private int vPerfLevel;
        svrPerfLevel(int value) { this.vPerfLevel = value; }
    }

    public class svrBeginParams
    {
        public int mainThreadId;
        public svrPerfLevel cpuPerfLevel;
        public svrPerfLevel gpuPerfLevel;
        public Surface surface;
        public int optionFlags;
        public svrColorSpace   colorSpace;
        public svrBeginParams(Surface mSurface) 
		{
            mainThreadId = 0;
            cpuPerfLevel = svrPerfLevel.kPerfMaximum;
            gpuPerfLevel = svrPerfLevel.kPerfMaximum;
            surface = mSurface;
            optionFlags = 0;
            colorSpace = svrColorSpace.kColorSpaceLinear;
        }
    }

    public enum svrFrameOption
    {
        kDisableDistortionCorrection(1),
        kDisableReprojection(2),
        kEnableMotionToPhoton(4),
        kDisableChromaticCorrection(8);

        private int vFrameOption;
        svrFrameOption(int value) { this.vFrameOption = value; }
        public int getFrameOption(){ return vFrameOption; }
    }

    public enum svrTextureType
    {
        kTypeTexture(0),
        kTypeTextureArray(1),
        kTypeImage(2),
        kTypeEquiRectTexture(3),
        kTypeEquiRectImage(4),
        kTypeVulkan(5);
        private int vTextureType;
        svrTextureType(int value) { this.vTextureType = value; }
    }

    public class svrVulkanTexInfo
    {
        public int memSize;
        public int width;
        public int height;
        public int numMips;
        public int bytesPerPixel;
        public int renderSemaphore;

        public svrVulkanTexInfo()
        {
            memSize = 0;
            width = 0;
            height = 0;
            numMips = 0;
            bytesPerPixel = 0;
            renderSemaphore = 0;
        }
    }
    public enum svrWarpType
    {
        kSimple(0);
        private int vWarpType;

        svrWarpType(int value) { this.vWarpType = value; }
    }
    public enum svrWarpMeshType
    {
        kMeshTypeColumsLtoR(0),
        kMeshTypeColumsRtoL(1),
        kMeshTypeRowsTtoB(2),
        kMeshTypeRowsBtoT(3);
        private int vWarpMeshType;
        svrWarpMeshType(int value) { this.vWarpMeshType = value; }
    }

    public enum svrWarpMeshEnum
    {
        kMeshEnumLeft(0),
        kMeshEnumRight(1),
        kMeshEnumUL(2),
        kMeshEnumUR(3),
        kMeshEnumLL(4),
        kMeshEnumLR(5),
        kWarpMeshCount(6);
        private int vWarpMeshEnum;
        svrWarpMeshEnum(int value) { this.vWarpMeshEnum = value; }
    }

    public class svrLayoutCoords
    {
        public float[] LowerLeftPos = new float[4];
        public float[] LowerRightPos = new float[4];
        public float[] UpperLeftPos = new float[4];
        public float[] UpperRightPos = new float[4];

        public float[] LowerUVs = new float[4];
        public float[] UpperUVs = new float[4];
        public float[] TransformMatrix = new float[16];

        public svrLayoutCoords()
        {
            float[] LowerLeftPos = {0.0f, 0.0f, 0.0f, 0.0f};
            float[] LowerRightPos = {0.0f, 0.0f, 0.0f, 0.0f};
            float[] UpperLeftPos = {0.0f, 0.0f, 0.0f, 0.0f};
            float[] UpperRightPos = {0.0f, 0.0f, 0.0f, 0.0f};
            float[] LowerUVs = {0.0f, 0.0f, 0.0f, 0.0f};
            float[] UpperUVs = {0.0f, 0.0f, 0.0f, 0.0f};
            float[] TransformMatrix =
                    {       0.0f, 0.0f, 0.0f, 0.0f,
                            0.0f, 0.0f, 0.0f, 0.0f,
                            0.0f, 0.0f, 0.0f, 0.0f,
                            0.0f, 0.0f, 0.0f, 0.0f
                    };
        }
    }

    public class svrRenderLayer
    {
        public int imageHandle;
        public svrTextureType imageType;
        public svrLayoutCoords imageCoords;
        public svrEyeMask eyeMask;
        public int layerFlags;
        public svrVulkanTexInfo vulkanInfo;

        public svrRenderLayer()
        {
            imageHandle = 0;
            imageType = svrTextureType.kTypeTexture;
            imageCoords = new svrLayoutCoords();
            eyeMask = svrEyeMask.kEyeMaskLeft;
            layerFlags = 0;
            vulkanInfo = new svrVulkanTexInfo();
        }
    }

    public class svrFrameParams
    {
        public int frameIndex;
        public int minVsyncs;
        public svrRenderLayer[] renderLayers = new svrRenderLayer[SVR_MAX_RENDER_LAYERS];
        public int frameOptions;
        public svrHeadPoseState headPoseState;
        public svrWarpType warpType;
        public float fieldOfView;

        public svrFrameParams()
        {
            frameIndex = 0;
            minVsyncs = 0;
            for (int i = 0; i < SVR_MAX_RENDER_LAYERS; i++)
            {
                renderLayers[i] = new svrRenderLayer();
            }
            frameOptions = 0;
            headPoseState = new svrHeadPoseState();
            warpType = svrWarpType.kSimple;
            fieldOfView = 0.0f;
        }
    }

    public class svrViewFrustum
    {
        public float left;
        public float right;
        public float top;
        public float bottom;

        public float near;
        public float far;

        public svrViewFrustum()
        {
            left = 0.0f;
            right = 0.0f;
            top = 0.0f;
            bottom = 0.0f;
            near = 0.0f;
            far = 0.0f;
        }
    }

    public class svrDeviceInfo
    {
        public int displayWidthPixels;
        public int displayHeightPixels;
        public float displayRefreshRateHz;
        public int displayOrientation;
        public int targetEyeWidthPixels;
        public int targetEyeHeightPixels;
        public float targetFovXRad;
        public float targetFovYRad;
        public svrViewFrustum leftEyeFrustum;
        public svrViewFrustum rightEyeFrustum;
        public float   targetEyeConvergence;
        public float   targetEyePitch;
        public int deviceOSVersion;
        public svrWarpMeshType warpMeshType;
    }

    public class svrEventData_Thermal
    {
        long thermalLevel;
    }

    public class svrEvent
    {
        svrEventType eventType;
        long deviceId;
        float eventTimeStamp;
        svrEventData_Thermal eventData;
    }
}
