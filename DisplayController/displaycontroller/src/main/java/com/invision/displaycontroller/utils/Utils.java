package com.invision.displaycontroller.utils;

import android.app.ActivityOptions;
import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.pm.ActivityInfo;
import android.content.pm.PackageManager;
import android.content.pm.ResolveInfo;
import android.graphics.drawable.Drawable;
import android.hardware.display.DisplayManager;
import android.net.Uri;
import android.os.Bundle;
import android.util.Log;
import android.view.Display;
import android.widget.Toast;

import com.invision.displaycontroller.ControlActivity;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;

public class Utils {

    public static HashMap<String, Drawable> mHashMap = new HashMap<>();

    public static HashMap<String, Drawable> getmHashMap() {
        return mHashMap;
    }

    public static void clearmHashMap() {
        Utils.mHashMap.clear();
    }

    public static List<Bundle> getListOfApps(Context paramContext) {
        PackageManager packageManager = paramContext.getPackageManager();
        Intent intent = new Intent("android.intent.action.MAIN");
        // intent.addCategory("com.qti.intent.category.SNAPDRAGON_VR");
        intent.addCategory("android.intent.category.LAUNCHER");
        List list = packageManager.queryIntentActivities(intent, PackageManager.GET_RESOLVED_FILTER);
        ArrayList<Bundle> arrayList = new ArrayList();
        for (int i = 0; i < list.size(); i++) {
            ActivityInfo activityInfo = ((ResolveInfo) list.get(i)).activityInfo;
            Bundle bundle = new Bundle();
            mHashMap.put(activityInfo.packageName, activityInfo.loadIcon(packageManager));
            bundle.putString("path", activityInfo.packageName);
            bundle.putString("label", activityInfo.loadLabel(packageManager).toString());
            arrayList.add(bundle);
        }
        return arrayList;
    }

    public static int getDisplayId(Context paramContext) {
        int displayId = -1;
        Display[] arrayOfDisplay = ((DisplayManager) paramContext.getSystemService(Context.DISPLAY_SERVICE)).getDisplays("android.hardware.display.category.PRESENTATION");

        int len = arrayOfDisplay.length;
        for (int i = 0; i < len; i++) {
            if (arrayOfDisplay[i].getDisplayId() != Display.DEFAULT_DISPLAY) {
                displayId = arrayOfDisplay[i].getDisplayId();
                break;
            }
        }

        return displayId;
    }

    public static void launchPackage(Context paramContext, Bundle paramBundle) {
        if (paramContext != null) {
            Intent intent = CreateIntentFromBundle(paramContext, paramBundle);
            paramContext.getPackageManager();
            if (intent != null) {
                Bundle bundle;
                byte b = 0;
                Display[] arrayOfDisplay = ((DisplayManager) paramContext.getSystemService(Context.DISPLAY_SERVICE)).getDisplays("android.hardware.display.category.PRESENTATION");
                int i = b;
                if (arrayOfDisplay != null) {
                    if (arrayOfDisplay.length > 0)
                        i = arrayOfDisplay[arrayOfDisplay.length - 1].getDisplayId();
                }

                Toast.makeText(paramContext, "Launching on Display [" + i + "]", Toast.LENGTH_LONG).show();
                bundle = ActivityOptions.makeBasic().setLaunchDisplayId(i).toBundle();
                /*
                if (i != 0) {
                    // 在主机端显示黑屏，不让控制其它东西
                    paramContext.startActivity(new Intent(paramContext.getApplicationContext(), ControlActivity.class), ActivityOptions.makeBasic().setLaunchDisplayId(0).toBundle());
                }
                */
                paramContext.startActivity(intent, bundle);
            }
        }
    }

    private static Intent CreateIntentFromBundle(Context paramContext, Bundle paramBundle) {
        Object object = null;
        String str = paramBundle.getString("path");
        if (str != null) {
            Intent intent = new Intent();
            object = paramBundle.getString("clazz");
            if (object != null) {
                Log.e("cwh", "getting Launch Intent for package+clazz - " + str + " -- " + object);
                intent.setComponent(new ComponentName(str, (String) object));
            } else {
                Log.e("cwh", "getting Launch Intent for package - " + str);
                intent = paramContext.getPackageManager().getLaunchIntentForPackage(str);
            }
            object = paramBundle.getString("data");
            if (object != null)
                try {
                    intent.setData(Uri.parse((String) object));
                } catch (Exception exception) {
                }
            object = intent;
            if (intent != null) {
                intent.putExtras(paramBundle);
                intent.addFlags(Intent.FLAG_ACTIVITY_MULTIPLE_TASK);
                intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
                object = intent;
            }
        }
        return (Intent) object;
    }
}
