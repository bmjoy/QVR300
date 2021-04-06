/*
 *  UVCCamera
 *  library and sample to access to UVC web camera on non-rooted Android device
 *
 * Copyright (c) 2014-2017 saki t_saki@serenegiant.com
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *   You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 *   Unless required by applicable law or agreed to in writing, software
 *   distributed under the License is distributed on an "AS IS" BASIS,
 *   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *   See the License for the specific language governing permissions and
 *   limitations under the License.
 *
 *  All files in the folder are under this Apache License, Version 2.0.
 *  Files in the libjpeg-turbo, libusb, libuvc, rapidjson folder
 *  may have a different license, see the respective files.
 */

package com.serenegiant.display;

import android.Manifest;
import android.app.Presentation;
import android.content.Context;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.graphics.Color;
import android.hardware.display.DisplayManager;
import android.opengl.GLSurfaceView;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.os.Process;
import android.support.v4.app.ActivityCompat;
import android.support.v7.app.AppCompatActivity;
import android.view.Display;
import android.view.ViewGroup;
import android.view.Window;
import android.view.WindowManager;
import android.widget.RelativeLayout;
import android.widget.Toast;

import com.serenegiant.helper.USBHelper;

public class MainActivity extends AppCompatActivity {
    private static final String TAG = "MainActivity";
    private static final String[] REQUEST_PERMISSIONS = {
            Manifest.permission.CAMERA,
            Manifest.permission.WRITE_EXTERNAL_STORAGE,
            Manifest.permission.READ_EXTERNAL_STORAGE,
    };
    private static final int PERMISSION_REQUEST_CODE = 1;

    private USBHelper usbHelper;
    private SecondDiaplay secondDiaplay;

    @Override
    protected void onCreate(final Bundle savedInstanceState) {
        requestWindowFeature(Window.FEATURE_NO_TITLE);
        getWindow().addFlags(WindowManager.LayoutParams.FLAG_FULLSCREEN);
        getWindow().addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);

        super.onCreate(savedInstanceState);

        usbHelper = new USBHelper();
        usbHelper.init(this);
    }

    @Override
    protected void onActivityResult(int requestCode, int resultCode, Intent data) {
        super.onActivityResult(requestCode, resultCode, data);
        if (requestCode == 2) {
            startSecondDisplay();
        }
    }

    @Override
    protected void onDestroy() {

        if (secondDiaplay != null)
            secondDiaplay.destroy();

        if (usbHelper != null)
            usbHelper.destroy();

        super.onDestroy();

        android.os.Process.killProcess(Process.myPid());
        System.exit(0);
    }


    private static class SecondDiaplay extends Presentation {

        private ViewGroup mRootView;
        private MyGLSurfaceView mGLSurfaceView;
        private MyGLRender mGLRender = new MyGLRender();

        public SecondDiaplay(Context outerContext, Display display) {
            super(outerContext, display);
        }

        @Override
        protected void onCreate(Bundle savedInstanceState) {


            super.onCreate(savedInstanceState);



//            setContentView(R.layout.activity_main);
//
//            mRootView = findViewById(R.id.container);

            start();
        }


        void start() {
            if (mGLSurfaceView == null) {
                mGLSurfaceView = new MyGLSurfaceView(getContext(), mGLRender);
                mGLSurfaceView.setRenderMode(GLSurfaceView.RENDERMODE_CONTINUOUSLY);
                mGLSurfaceView.setPreserveEGLContextOnPause(true);
                new Thread(new Runnable() {
                    @Override
                    public void run() {
                        CommonUtils.copyAssetsDirToSDCard(getContext(), "poly", "/sdcard/model");
                        CommonUtils.copyAssetsDirToSDCard(getContext(), "arconfig", "/sdcard/model");
//                        mGLRender.init();

                        new Handler(Looper.getMainLooper()).post(new Runnable() {
                            @Override
                            public void run() {
//                                RelativeLayout.LayoutParams lp = new RelativeLayout.LayoutParams(
//                                        ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.MATCH_PARENT);
//                                lp.addRule(RelativeLayout.CENTER_IN_PARENT);
//
//                                mRootView.addView(mGLSurfaceView, lp);
//                                mRootView.setAlpha(0.1f);

//                                mGLSurfaceView.setBackgroundColor(Color.parseColor("#00000000"));
//                                mGLSurfaceView.setAlpha(0.1f);
                                setContentView(mGLSurfaceView);
                            }
                        });
                    }
                }).start();
            }
        }

        void destroy() {
//            mGLRender.unInit();
            if (mRootView != null) {
                mRootView.removeView(mGLSurfaceView);
            }
        }
    }


    @Override
    protected void onResume() {
        super.onResume();
        if (!hasPermissionsGranted(REQUEST_PERMISSIONS)) {
            ActivityCompat.requestPermissions(this, REQUEST_PERMISSIONS, PERMISSION_REQUEST_CODE);
        } else {
            startSecondDisplay();
        }
    }

    private void startSecondDisplay() {
        if (secondDiaplay == null) {
            DisplayManager mDisplayManager;//屏幕管理类
            Display[] displays;//屏幕数组
            mDisplayManager = (DisplayManager) getSystemService(Context.DISPLAY_SERVICE);
            displays = mDisplayManager.getDisplays();
            if (displays.length > 1) {
                secondDiaplay = new SecondDiaplay(this, displays[1]);
//                secondDiaplay.getWindow().
                secondDiaplay.show();
            } else {

            }

        }
    }

    protected boolean hasPermissionsGranted(String[] permissions) {
        for (String permission : permissions) {
            if (ActivityCompat.checkSelfPermission(this, permission)
                    != PackageManager.PERMISSION_GRANTED) {
                return false;
            }
        }
        return true;
    }

    @Override
    public void onRequestPermissionsResult(int requestCode, String[] permissions, int[] grantResults) {
        if (requestCode == PERMISSION_REQUEST_CODE) {
            if (!hasPermissionsGranted(REQUEST_PERMISSIONS)) {
                Toast.makeText(this, "We need the permission: WRITE_EXTERNAL_STORAGE", Toast.LENGTH_SHORT).show();
            } else {

            }
        } else {
            super.onRequestPermissionsResult(requestCode, permissions, grantResults);
        }
    }


}
