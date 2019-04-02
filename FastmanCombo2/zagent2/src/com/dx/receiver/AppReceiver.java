package com.dx.receiver;

import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.os.AsyncTask;
import android.support.v4.content.LocalBroadcastManager;
import android.text.TextUtils;

import com.dx.consts.Action;
import com.dx.consts.Const;
import com.dx.service.ZMsg2;
import com.dx.utils.Log;
import com.dx.utils.SpUtil;

public class AppReceiver extends BroadcastReceiver {
    final String TAG = AppReceiver.class.getSimpleName();

    @Override
    public void onReceive(Context context, Intent intent) {
        String action = intent.getAction();
        String pkg = intent.getData().getSchemeSpecificPart();
        if (TextUtils.isEmpty(pkg) || context.getPackageName().equals(pkg)) {
            return;
        }

        SpUtil spUtil = SpUtil.getInstance(context);
        String allStartApps = spUtil.getAllPkgs();

        if (Intent.ACTION_PACKAGE_ADDED.equals(action)) {
            spUtil.putPkg(SpUtil.SP_INSTALLED_APPS, pkg);
            if (allStartApps.contains(pkg)) {
                // addLog(spUtil, context, "NeedStartApp - [" + pkg + "]");
                spUtil.putPkg(SpUtil.SP_NEEDS_START_APPS, pkg);
            }
        }
    }

    class StartAppTask extends AsyncTask<Void, Void, Void> {

        Context context;
        String pkg;
        int count;

        public StartAppTask(Context context, String pkg, int count) {
            this.context = context;
            this.pkg = pkg;
            this.count = count;
        }

        @Override
        protected Void doInBackground(Void... params) {
            try {
                PackageManager pm = context.getPackageManager();
                Intent it = pm.getLaunchIntentForPackage(pkg);
                it.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
                for (int i = 0; i < count; i++) {
                    context.startActivity(it);
                    Log.e(TAG, "StartAppTask " + (i + 1) + ":" + pkg);
                    Thread.sleep(5000);
                }
            } catch (Exception e) {
                e.printStackTrace();
            }
            return null;
        }
    }

    void addLog(SpUtil spUtil, Context context, String log) {
        if (spUtil.isYunOs()) {
            Intent itBroadcast = new Intent(Action.DX_LOG);
            itBroadcast.putExtra(Const.NEW_LOG, log);
            context.sendBroadcast(itBroadcast);
        } else {
            Intent intent = new Intent(ZMsg2.ZMSG2_BROADCAST_ACTION);
            intent.putExtra("cmd", ZMsg2.ZMSG2_CMD_ADD_LOG);
            intent.putExtra("time", System.currentTimeMillis());
            intent.putExtra("log", log);
            LocalBroadcastManager.getInstance(context).sendBroadcast(intent);
        }
    }

}
