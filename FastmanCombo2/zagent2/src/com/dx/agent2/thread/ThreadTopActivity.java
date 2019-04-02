package com.dx.agent2.thread;

import java.util.List;

import android.app.ActivityManager;
import android.app.ActivityManager.RunningTaskInfo;
import android.content.Context;
import android.content.Intent;
import android.support.v4.content.LocalBroadcastManager;

import com.dx.agent2.LoadActivity;
import com.dx.service.ZMsg2;
import com.dx.utils.Log;
import com.dx.utils.ZThreadPool;
import com.dx.utils.ZThreadPool.ZThread;

public class ThreadTopActivity extends ZThread {
    public static final String TAG = "ThreadTopActivity";

    private Context mContext;
    ActivityManager am;
    private LocalBroadcastManager lbm;
    private boolean needsQuit = false;

    public ThreadTopActivity(Context context, ZThreadPool parent) {
        super(parent, TAG);
        this.mContext = context;
        this.am = (ActivityManager) mContext.getSystemService(Context.ACTIVITY_SERVICE);
        this.lbm = LocalBroadcastManager.getInstance(mContext);
    }

    @Override
    public void core_run() throws Exception {
        while (!needsQuit) {
            // 置顶日志界面
            Intent it = new Intent(mContext, LoadActivity.class);
            it.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
            it.putExtra("needsCheckZmLog", false);
            mContext.startActivity(it);
            // addLog("置顶日志界面");
            Thread.sleep(5 * 1000);// 5秒后检测
            // needsQuit = topActivityIsEquals(am,
            // "com.dx.agent2.MainActivity");
        }
    }

    void addLog(String log) {
        Intent intent = new Intent(ZMsg2.ZMSG2_BROADCAST_ACTION);
        intent.putExtra("cmd", ZMsg2.ZMSG2_CMD_ADD_LOG);
        intent.putExtra("time", System.currentTimeMillis());
        intent.putExtra("log", log);
        lbm.sendBroadcast(intent);
    }

    @SuppressWarnings("deprecation")
    boolean topActivityIsEquals(ActivityManager am, String name) {
        // needs add android.permission.GET_TASKS
        List<RunningTaskInfo> runningTaskInfos = am.getRunningTasks(1);
        if (runningTaskInfos != null) {
            String topName = runningTaskInfos.get(0).topActivity.getClassName();
            Log.e(TAG, "TopActivity:" + topName);
            return name.equals(topName);
        } else {
            return false;
        }
    }

}
