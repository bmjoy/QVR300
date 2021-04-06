package com.invision.displaycontroller;

import android.content.Context;
import android.hardware.display.DisplayManager;
import android.hardware.input.InputManager;
import android.os.Bundle;
import android.util.Log;
import android.view.Display;
import android.view.InputDevice;
import android.view.InputEvent;
import android.view.KeyEvent;
import android.view.MotionEvent;
import android.view.View;
import android.view.WindowManager;
import android.widget.Button;

import androidx.appcompat.app.AppCompatActivity;

import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Method;

public class ControlActivity extends AppCompatActivity {

    private static final int DOWN_TIME = 50;
    private static final long EVENT_TIME = 100;
    private static final int ACTION = KeyEvent.ACTION_DOWN;
    private static final int KEYCODE = KeyEvent.KEYCODE_BACK;
    private static final int REPEAT = 0;
    private static final int METASTATE = 0;
    private static final int DEVICE_ID = 0;
    private static final int SCAN_CODE = 0;
    private static final int FLAGS = 0;
    private static final int SOURCE = InputDevice.SOURCE_KEYBOARD;
    private static final String CHARACTERS = null;

    private boolean isLockLongPressKey = false;

    private Button mBack;

    private void hideSystemUI() {
        getWindow().getDecorView().setSystemUiVisibility(View.SYSTEM_UI_FLAG_FULLSCREEN);
    }

    private void showSystemUI() {
        getWindow().getDecorView().setSystemUiVisibility(View.SYSTEM_UI_FLAG_VISIBLE);
    }
    @Override
    public boolean onTouchEvent(MotionEvent event) {
        Log.e("@@@","B OnTouch !!");
        return super.onTouchEvent(event);
    }
    @Override
    protected void onCreate(Bundle paramBundle) {
        super.onCreate(paramBundle);
        setContentView(R.layout.activity_control);
        getWindow().setFlags(WindowManager.LayoutParams.FLAG_FULLSCREEN, WindowManager.LayoutParams.FLAG_FULLSCREEN);
        //getWindow().setFlags(WindowManager.LayoutParams.FLAG_NOT_TOUCHABLE, WindowManager.LayoutParams.FLAG_NOT_TOUCHABLE);
        Log.d("david", "onCreate");
        mBack = findViewById(R.id.back);

        mBack.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                Log.d("david", "back key");
                onBack();
            }
        });


        try {
            Method forName = Class.class.getDeclaredMethod("forName", String.class);
            Method getDeclaredMethod = Class.class.getDeclaredMethod("getDeclaredMethod", String.class, Class[].class);
            Class<?> vmRuntimeClass = (Class<?>) forName.invoke(null, "dalvik.system.VMRuntime");
            Method getRuntime = (Method) getDeclaredMethod.invoke(vmRuntimeClass, "getRuntime", null);
            Method setHiddenApiExemptions = (Method) getDeclaredMethod.invoke(vmRuntimeClass, "setHiddenApiExemptions", new Class[]{String[].class});
            Object sVmRuntime = getRuntime.invoke(null);
            setHiddenApiExemptions.invoke(sVmRuntime, new Object[]{new String[]{"L"}});
        } catch (Throwable e) {
            Log.e("[error]", "reflect bootstrap failed:", e);
        }
    }

    public void onBack() {
        // KeyEvent key = new KeyEvent(KeyEvent.ACTION_DOWN, KeyEvent.KEYCODE_BACK);
        KeyEvent key = getKeyEvent();
        InputManager inputManager = (InputManager) getSystemService(Context.INPUT_SERVICE);
        invokeInjectInputEventMethod(inputManager, key, 0);
    }

    private void invokeInjectInputEventMethod(InputManager inputManager, InputEvent event, int mode) {
        Class<?> clazz = null;
        Method injectInputEventMethod = null;
        Method recycleMethod = null;

        try {
            clazz = Class.forName("android.hardware.input.InputManager");
        } catch (ClassNotFoundException e) {
            e.printStackTrace();
        }

        try {
            injectInputEventMethod = clazz.getMethod("injectInputEvent", InputEvent.class, int.class);
        } catch (NoSuchMethodException e) {
            e.printStackTrace();
        }

        try {
            injectInputEventMethod.invoke(inputManager, event, mode);
            // 准备回收event的方法
            recycleMethod = event.getClass().getMethod("recycle");
            //执行event的recycle方法
            recycleMethod.invoke(event);
        } catch (IllegalAccessException e) {
            e.printStackTrace();
        } catch (InvocationTargetException e) {
            e.printStackTrace();
        } catch (NoSuchMethodException e) {
            e.printStackTrace();
        }
    }

    public KeyEvent getKeyEvent() {
        try {
            byte b = 0;
            Display[] arrayOfDisplay = ((DisplayManager) getSystemService(Context.DISPLAY_SERVICE)).getDisplays("android.hardware.display.category.PRESENTATION");
            int i = b;
            if (arrayOfDisplay != null) {
                if (arrayOfDisplay.length > 0)
                    i = arrayOfDisplay[0].getDisplayId();
            }

            Class clazz = Class.forName("android.view.KeyEvent");
            Method getter = clazz.getMethod("obtain", new Class[]{long.class, long.class, int.class, int.class, int.class, int.class, int.class, int.class, int.class, int.class, int.class, String.class});//方法名，参数类型
            KeyEvent result = (KeyEvent) getter.invoke(new KeyEvent(KeyEvent.ACTION_DOWN, KeyEvent.KEYCODE_BACK), DOWN_TIME, EVENT_TIME, ACTION, KEYCODE, REPEAT,
                    METASTATE, DEVICE_ID, SCAN_CODE, FLAGS, SOURCE, i, CHARACTERS);

            return result;
        } catch (Exception e) {
            e.printStackTrace();
        }

        return new KeyEvent(KeyEvent.ACTION_DOWN, KeyEvent.KEYCODE_BACK);
    }

    public void onWindowFocusChanged(boolean paramBoolean) {
        super.onWindowFocusChanged(paramBoolean);
        showSystemUI();
    }

    public boolean onKeyDown(int keyCode, KeyEvent event) {
        switch (keyCode) {
            case KeyEvent.KEYCODE_HOME:
                return true;
            case KeyEvent.KEYCODE_VOLUME_DOWN:
            case KeyEvent.KEYCODE_VOLUME_UP:
                if (event.getRepeatCount() == 0) {
                    event.startTracking();
                    isLockLongPressKey = false;
                } else {
                    isLockLongPressKey = true;
                }
                return true;
            case KeyEvent.KEYCODE_BACK:
                Log.i("david", "do nothing");
                return true;
        }
        return super.onKeyDown(keyCode, event);
    }

    @Override
    public boolean onKeyUp(int keyCode, KeyEvent event) {
        switch (keyCode) {
            case KeyEvent.KEYCODE_VOLUME_DOWN: {
                if (!isLockLongPressKey) {
                    // finish();
                }
                return true;
            }
        }
        return super.onKeyUp(keyCode, event);
    }

    @Override
    protected void onStop() {
        super.onStop();
        finish();
    }
}
