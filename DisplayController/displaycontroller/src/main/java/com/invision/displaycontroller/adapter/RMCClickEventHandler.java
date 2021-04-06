package com.invision.displaycontroller.adapter;

import android.app.Activity;
import android.content.Intent;
import android.os.Bundle;
import android.util.Log;
import android.view.View;

import com.invision.displaycontroller.SecondActivity;
import com.invision.displaycontroller.interfaces.RecyclerViewEventListener;
import com.invision.displaycontroller.utils.Utils;

public class RMCClickEventHandler implements RecyclerViewEventListener {
    public void onRVItemClick(View paramView, Bundle paramBundle) {
        if(paramBundle.getString("label").equals("SimpleVR")){
            Intent intent = new Intent(paramView.getContext(), SecondActivity.class);
            intent.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
            paramView.getContext().startActivity(intent);

        }
        Utils.launchPackage(paramView.getContext(), paramBundle);

    }
}
