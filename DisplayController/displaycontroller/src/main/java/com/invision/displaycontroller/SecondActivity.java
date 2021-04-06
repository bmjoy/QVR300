package com.invision.displaycontroller;

import android.app.Activity;
import android.content.Intent;
import android.os.Bundle;
import android.util.Log;
import android.view.MotionEvent;
import android.view.View;
import android.widget.Button;

import androidx.annotation.Nullable;

public class SecondActivity extends Activity {
    private  Button btnSend;
    private  Button btnUpdatePlan;
    @Override
    protected void onCreate(@Nullable Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.second);
        btnSend = findViewById(R.id.btn_Send);
        btnUpdatePlan = findViewById(R.id.btn_Plan);
        btnSend.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                Log.e("@@@","send!");
                Intent intent = new Intent();
                intent.setAction("boardcast_for_update_anchor");
                sendBroadcast(intent);
            }
        });
        btnUpdatePlan.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                Log.e("@@@","send!");
                Intent intent = new Intent();
                intent.setAction("boardcast_for_update_plan");
                sendBroadcast(intent);
            }
        } );
    }


}
