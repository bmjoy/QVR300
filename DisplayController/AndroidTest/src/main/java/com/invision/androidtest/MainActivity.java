package com.invision.androidtest;

import android.app.Notification;
import android.app.NotificationChannel;
import android.app.NotificationManager;
import android.app.PendingIntent;
import android.content.Context;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.graphics.BitmapFactory;
import android.os.Bundle;
import android.util.Log;
import android.view.KeyEvent;
import android.view.MotionEvent;
import android.view.View;
import android.widget.Button;

import androidx.appcompat.app.AppCompatActivity;
import androidx.core.app.NotificationCompat;

public class MainActivity extends AppCompatActivity {

    private final String TEST_NOTIFICATION_CHANNEL = "test_notification_channel";

    private Button mButton;

    private int id = 0;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);

        mButton = findViewById(R.id.send_notification);

        mButton.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                /*
                //创建一个message通道，名字为消息
                createNotificationChannel("message", "消息", NotificationManager.IMPORTANCE_HIGH);

                sendNotification(id++,"Test", "String content sdfdsfdsfsdfsd");
                */
                Log.i("david", "click");
                id++;
                mButton.setText("Click:" + id);
            }
        });

        /*
        Intent intent = new Intent();
        intent.setAction(Intent.ACTION_SEND);
        intent.setType("text/plain");

        startActivity(Intent.createChooser(intent, "对话框标题"));

        startActivity(intent);
        */
        PackageManager pm = getPackageManager();
        boolean telephony = pm.hasSystemFeature(PackageManager.FEATURE_TELEPHONY);

        Log.i("David", "is support telephony=" + telephony);

        boolean cdma = pm.hasSystemFeature(PackageManager.FEATURE_TELEPHONY_CDMA);

        Log.i("David", "is support cdma=" + cdma);

        boolean gsm = pm.hasSystemFeature(PackageManager.FEATURE_TELEPHONY_GSM);

        Log.i("David", "is support gsm=" + gsm);
    }

    private void sendNotification(int id, String title, String content) {
        Intent intent = new Intent(this, MainActivity.class);
        PendingIntent pendingIntent = PendingIntent.getActivity(this, R.string.app_name, intent, PendingIntent.FLAG_UPDATE_CURRENT);
        NotificationCompat.Builder builder = new NotificationCompat.Builder(this, "message");
        builder.setContentIntent(pendingIntent).setAutoCancel(true).setSmallIcon(R.drawable.ic_launcher_background).setTicker("提示消息").setWhen(System.currentTimeMillis())
                .setLargeIcon(BitmapFactory.decodeResource(this.getResources(), R.drawable.ic_launcher_foreground)).setContentTitle(title).setContentText(content);
        Notification notification = builder.build();
        NotificationManager notificationManager = (NotificationManager) this.getSystemService(Context.NOTIFICATION_SERVICE);
        notificationManager.notify(id, notification);
    }

    private void createNotificationChannel(String channelId, String channelName, int importance) {
        NotificationChannel notificationChannel = new NotificationChannel(channelId, channelName, importance);
        NotificationManager notificationManager  = (NotificationManager)this.getSystemService(Context.NOTIFICATION_SERVICE);
        notificationManager.createNotificationChannel(notificationChannel);
    }

    @Override
    protected void onResume() {
        Log.i("David", "onResume");
        super.onResume();
    }

    @Override
    protected void onPause() {
        Log.i("David", "onPause");
        super.onPause();
    }

    @Override
    public void onWindowFocusChanged(boolean hasFocus) {
        Log.i("David", "onWindowFocusChanged = " + hasFocus);
        super.onWindowFocusChanged(hasFocus);
    }

    @Override
    public boolean onKeyDown(int keyCode, KeyEvent event) {
        Log.i("David_Key", "keyCode=" + keyCode + "|KeyEvent=" + event.toString());
        return super.onKeyDown(keyCode, event);
    }

    @Override
    public boolean dispatchKeyEvent(KeyEvent event) {
        Log.i("David_Key", "dispatchKeyEvent KeyEvent=" + event.toString());
        return super.dispatchKeyEvent(event);
    }

    @Override
    public boolean onGenericMotionEvent(MotionEvent event) {
        Log.i("David_Key", "onGenericMotionEvent MotionEvent=" + event.toString());
        return super.onGenericMotionEvent(event);
    }

    @Override
    public boolean onTrackballEvent(MotionEvent event) {
        Log.i("David_Key", "onTrackballEvent MotionEvent=" + event.toString());
        return super.onTrackballEvent(event);
    }

    @Override
    public boolean dispatchGenericMotionEvent(MotionEvent ev) {
        Log.i("David_Key", "dispatchGenericMotionEvent MotionEvent=" + ev.toString());
        return super.dispatchGenericMotionEvent(ev);
    }
}
