package com.invision.displaycontroller.adapter;

import android.content.Context;
import android.os.Bundle;
import android.util.Log;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ImageView;
import android.widget.TextView;

import androidx.recyclerview.widget.RecyclerView;

import com.invision.displaycontroller.R;
import com.invision.displaycontroller.interfaces.RecyclerViewEventListener;
import com.invision.displaycontroller.utils.Utils;

import java.lang.ref.WeakReference;
import java.util.ArrayList;
import java.util.List;

public class RMCRecyclerViewAdapter extends RecyclerView.Adapter<RMCRecyclerViewAdapter.ItemViewHolder> {
    List<Bundle> appList;

    WeakReference<Context> contextWeakReference;

    RecyclerViewEventListener listener;

    public RMCRecyclerViewAdapter(RecyclerViewEventListener paramRecyclerViewEventListener) {
        this.listener = paramRecyclerViewEventListener;
        this.appList = new ArrayList<Bundle>();
    }

    public int getItemCount() {
        return (this.appList != null) ? this.appList.size() : 0;
    }

    public List<Bundle> getList() {
        return this.appList;
    }

    public void onBindViewHolder(ItemViewHolder paramItemViewHolder, int paramInt) {
        try {
            String str = (this.appList.get(paramInt)).getString("path", "");
            paramItemViewHolder.icon.setImageDrawable(Utils.getmHashMap().get(str));
            str = (this.appList.get(paramInt)).getString("label", "");
            paramItemViewHolder.name.setText(str);
        } catch (Exception exception) {
            exception.printStackTrace();
        }
    }

    public ItemViewHolder onCreateViewHolder(ViewGroup paramViewGroup, int paramInt) {
        this.contextWeakReference = new WeakReference<Context>(paramViewGroup.getContext());
        return new ItemViewHolder(LayoutInflater.from(this.contextWeakReference.get()).inflate(R.layout.recycleview_item, paramViewGroup, false), this.listener);
    }

    public void setList(List<Bundle> paramList) {
        this.appList = paramList;
        notifyDataSetChanged();
    }

    public class ItemViewHolder extends RecyclerView.ViewHolder implements View.OnClickListener {
        public ImageView icon;

        public RecyclerViewEventListener listener;

        public TextView name;

        ItemViewHolder(View param1View, RecyclerViewEventListener param1RecyclerViewEventListener) {
            super(param1View);
            this.listener = param1RecyclerViewEventListener;
            this.icon = param1View.findViewById(R.id.appicon);
            this.name = param1View.findViewById(R.id.appname);
            param1View.setOnClickListener(this);
        }

        public void onClick(View param1View) {
            this.listener.onRVItemClick(param1View, RMCRecyclerViewAdapter.this.appList.get(getAdapterPosition()));
        }
    }
}
