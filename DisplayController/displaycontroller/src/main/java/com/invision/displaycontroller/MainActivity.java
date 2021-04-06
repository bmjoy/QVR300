package com.invision.displaycontroller;

import android.app.LoaderManager;
import android.content.Loader;
import android.hardware.usb.UsbDevice;
import android.os.Bundle;
import android.util.Log;
import android.view.KeyEvent;
import android.view.MotionEvent;

import androidx.appcompat.app.AppCompatActivity;
import androidx.recyclerview.widget.GridLayoutManager;
import androidx.recyclerview.widget.RecyclerView;

import com.invision.displaycontroller.adapter.RMCClickEventHandler;
import com.invision.displaycontroller.adapter.RMCRecyclerViewAdapter;
import com.invision.displaycontroller.loader.AppInfoLoader;
import com.invision.displaycontroller.utils.Utils;

import java.util.List;

public class MainActivity extends AppCompatActivity implements LoaderManager.LoaderCallbacks<List<Bundle>> {

    private RMCRecyclerViewAdapter mAdapter;

    private RMCClickEventHandler rmcClickEventHandler;

    private RecyclerView mRecyclerView;

    private UsbDevice mVrDevice;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);

        mRecyclerView = findViewById(R.id.rcv);
        rmcClickEventHandler = new RMCClickEventHandler();
        GridLayoutManager gridLayoutManager = new GridLayoutManager(getApplicationContext(), 4);
        mRecyclerView.setLayoutManager(gridLayoutManager);
        mAdapter = new RMCRecyclerViewAdapter(rmcClickEventHandler);
        mRecyclerView.setAdapter(mAdapter);

        getLoaderManager().restartLoader(1, null, MainActivity.this).forceLoad();
    }

    @Override
    public boolean onTouchEvent(MotionEvent event) {
        Log.e("@@@","A OnTouch !!");
        return super.onTouchEvent(event);
    }

    @Override
    public Loader<List<Bundle>> onCreateLoader(int id, Bundle args) {
        return new AppInfoLoader(getApplicationContext());
    }

    @Override
    public void onLoadFinished(Loader<List<Bundle>> loader, List<Bundle> data) {
        mAdapter.setList(data);
    }

    @Override
    public void onLoaderReset(Loader<List<Bundle>> loader) {
    }

    @Override
    protected void onDestroy() {
        Utils.clearmHashMap();
        super.onDestroy();
    }
}