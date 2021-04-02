//-----------------------------------------------------------------------------
//  Copyright (c) 2017 Qualcomm Technologies, Inc.
//  All Rights Reserved. Qualcomm Technologies Proprietary and Confidential.
//-----------------------------------------------------------------------------
package com.qualcomm.svr.apps.photoViewer;

import android.app.Activity;
import android.app.ActivityManager;
import android.content.Context;
import android.content.pm.ConfigurationInfo;
import android.os.Bundle;
import android.util.Log;
import android.view.View;
import android.view.WindowManager;

public class photoViewer extends Activity {
    @Override
    public void onCreate(Bundle savedInstanceState) {
        Log.i(TAG, "onCreate");
        super.onCreate(savedInstanceState);
        getWindow().getDecorView().setSystemUiVisibility(
                View.SYSTEM_UI_FLAG_LAYOUT_STABLE
                        | View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION
                        | View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN
                        | View.SYSTEM_UI_FLAG_HIDE_NAVIGATION
                        | View.SYSTEM_UI_FLAG_FULLSCREEN
                        | View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY);
        getWindow().addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
        Context context = this.getApplicationContext();
        mSvrView = new svrView(context, this);

        // Check if the system supports OpenGL ES 3.0.
        final ActivityManager activityManager = (ActivityManager) getSystemService(Context.ACTIVITY_SERVICE);
        final ConfigurationInfo configurationInfo = activityManager.getDeviceConfigurationInfo();
        final boolean supportsEs3 = configurationInfo.reqGlEsVersion >= 0x30000;
        Log.i(TAG, "OpenGL ES Version is " + configurationInfo.reqGlEsVersion);
        if (supportsEs3) {
            // Request an OpenGL ES 3.0 compatible context.
            mSvrView.setEGLContextClientVersion(3);
            mSvrView.setSvrRenderer(new photoViewerRenderer(this, context));
            CreateLayoutCoords(0.0f, 0.0f, 1.0f, 1.0f);
            mSvrView.setLayoutCoords(lowerLeftPos, lowerRightPos, upperLeftPos, upperRightPos, lowerUVs, upperUVs, transformMatrix);
            mSvrView.setFrameParams();
        } else {
            // svr target for ES 3 at minimum.
            return;
        }
        setContentView(mSvrView);
    }

    @Override
    protected void onResume() {
        super.onResume();
        mSvrView.onResume();
    }

    @Override
    protected void onPause() {
        super.onPause();
        mSvrView.onPause();
    }

    @Override
    protected void onStop() {
        super.onStop();
    }

    @Override
    public void onWindowFocusChanged(boolean hasFocus) {
        if (android.os.Build.VERSION.SDK_INT >= 19) {
            if (hasFocus) {
                getWindow().getDecorView().setSystemUiVisibility(
                        View.SYSTEM_UI_FLAG_LAYOUT_STABLE
                                | View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION
                                | View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN
                                | View.SYSTEM_UI_FLAG_HIDE_NAVIGATION
                                | View.SYSTEM_UI_FLAG_FULLSCREEN
                                | View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY);
                getWindow().addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
            }
        }
    }

    public void CreateLayoutCoords(float centerX, float centerY, float radiusX, float radiusY) {
        // This is always in screen space so we want Z = 0 and W = 1
        float[] mLowerLeftPos = {centerX - radiusX, centerY - radiusY, 0.0f, 1.0f};

        float[] mLowerRightPos = {centerX + radiusX, centerY - radiusY, 0.0f, 1.0f};
        float[] mUpperLeftPos = {centerX - radiusX, centerY + radiusY, 0.0f, 1.0f};
        float[] mUpperRightPos = {centerX + radiusX, centerY + radiusY, 0.0f, 1.0f};

        float[] mLowerUVs = {0.0f, 0.0f, 1.0f, 0.0f};
        float[] mUpperUVs = {0.0f, 1.0f, 1.0f, 1.0f};

        lowerLeftPos = mLowerLeftPos;
        lowerRightPos = mLowerRightPos;
        upperLeftPos = mUpperLeftPos;
        upperRightPos = mUpperRightPos;
        lowerUVs = mLowerUVs;
        upperUVs = mUpperUVs;
    }

    private static final String TAG = "photoViewer";
    private svrView mSvrView;

    float[] lowerLeftPos;

    float[] lowerRightPos;
    float[] upperLeftPos;
    float[] upperRightPos;

    float[] lowerUVs;
    float[] upperUVs;
    float[] transformMatrix =
            {1.0f, 0.0f, 0.0f, 0.0f,
                    0.0f, 1.0f, 0.0f, 0.0f,
                    0.0f, 0.0f, 1.0f, 0.0f,
                    0.0f, 0.0f, 0.0f, 1.0f};

}