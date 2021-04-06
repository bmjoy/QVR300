package com.invision.displaycontroller.loader;

import android.content.AsyncTaskLoader;
import android.content.Context;
import android.os.Bundle;

import com.invision.displaycontroller.utils.Utils;

import java.util.List;

public class AppInfoLoader extends AsyncTaskLoader<List<Bundle>> {
    public AppInfoLoader(Context paramContext) {
        super(paramContext);
    }

    public List<Bundle> loadInBackground() {
        return Utils.getListOfApps(getContext());
    }
}
