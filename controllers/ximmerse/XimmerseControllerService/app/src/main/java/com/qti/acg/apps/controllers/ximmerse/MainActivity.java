package com.qti.acg.apps.controllers.ximmerse;

import android.app.Activity;
import android.content.Intent;
import android.os.Bundle;

public class MainActivity extends Activity {

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        Intent intent = new Intent();
        intent.setAction("android.intent.action.MAIN");
        intent.setPackage("com.ximmerse.xtools.bt_bind");
        intent.setClassName("com.ximmerse.xtools.bt_bind", "com.ximmerse.xtools.MainActivity");
        startActivity(intent);
        finish();
    }
}
