//=============================================================================
// FILE: SvrNativeActivity.java
//
//                  Copyright (c) 2017 QUALCOMM Technologies Inc.
//                              All Rights Reserved.
//
//=============================================================================
package com.qualcomm.svr.simple;

import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.content.SharedPreferences;
import android.os.Process;
import android.preference.PreferenceManager;
import android.util.Log;
import android.view.View;
import android.os.Bundle;
import 	android.content.res.AssetManager;
import android.view.WindowManager;
//import com.serenegiant.helper.USBHelper;
import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;

public class SvrNativeActivity extends android.app.NativeActivity
{
	static
	{
		System.loadLibrary( "svrapi" );
		System.loadLibrary( "simple" );
	}
	public static native void NativeUpdateAnchor();
	public static native void NativeUpdatePlan();
	public static final String FIRST_TIME_TAG = "first_time";
	public static final  String ASSETS_SUB_FOLDER_NAME = "raw";
	public static final int BUFFER_SIZE = 1024;

//	private USBHelper usbHelper;
	private BroadcastReceiver updateAnchorReceiver = new BroadcastReceiver() {
		@Override
		public void onReceive(Context context, Intent intent) {
			Log.e("@@@","NativeUpdateAnchor");
			NativeUpdateAnchor();
		}
	};
	private BroadcastReceiver updatePlanReceiver = new BroadcastReceiver() {
		@Override
		public void onReceive(Context context, Intent intent) {
			Log.e("@@@","NativeUpdatePlan");
			NativeUpdatePlan();
		}
	};
	@Override
	public void onWindowFocusChanged (boolean hasFocus)
	{
		if(android.os.Build.VERSION.SDK_INT >= 19) 
		{
			if(hasFocus) 
			{
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

	@Override
	protected void onCreate(Bundle savedInstanceState) {
		super.onCreate(savedInstanceState);
		SharedPreferences prefs = PreferenceManager.getDefaultSharedPreferences(this);
		if (!prefs.getBoolean(FIRST_TIME_TAG, false)) {
			SharedPreferences.Editor editor = prefs.edit();
			editor.putBoolean(FIRST_TIME_TAG, true);
			editor.commit();
			copyAssetsToExternal();
		}
//		usbHelper = new USBHelper();
//		usbHelper.init(this);
		IntentFilter filter = new IntentFilter();
		filter.addAction("boardcast_for_update_anchor");
		registerReceiver(updateAnchorReceiver, filter);

		IntentFilter filter2 = new IntentFilter();
		filter2.addAction("boardcast_for_update_plan");
		registerReceiver(updatePlanReceiver, filter2);
	}

	/*
	 * copy the Assets from assets/raw to app's external file dir
	 */
	public void copyAssetsToExternal() {
		AssetManager assetManager = getAssets();
		String[] files = null;
		try {
			InputStream in = null;
			OutputStream out = null;

			files = assetManager.list(ASSETS_SUB_FOLDER_NAME);
			for (int i = 0; i < files.length; i++) {
				in = assetManager.open(ASSETS_SUB_FOLDER_NAME + "/" + files[i]);
				String outDir = getExternalFilesDir(null).toString() + "/";

				File outFile = new File(outDir, files[i]);
				out = new FileOutputStream(outFile);
				copyFile(in, out);
				in.close();
				in = null;
				out.flush();
				out.close();
				out = null;
			}
		} catch (IOException e) {
			Log.e("tag", "Failed to get asset file list.", e);
		}
		File file = getExternalFilesDir(null);
		Log.d("tag", "file:" + file.toString());
	}
    /*
     * read file from InputStream and write to OutputStream.
     */
	private void copyFile(InputStream in, OutputStream out) throws IOException {
		byte[] buffer = new byte[BUFFER_SIZE];
		int read;
		while ((read = in.read(buffer)) != -1) {
			out.write(buffer, 0, read);
		}
	}

	@Override
	protected void onDestroy() {

//		if (usbHelper != null){
//			usbHelper.destroy();
//		}

		super.onDestroy();

		android.os.Process.killProcess(Process.myPid());
		System.exit(0);
	}
}
