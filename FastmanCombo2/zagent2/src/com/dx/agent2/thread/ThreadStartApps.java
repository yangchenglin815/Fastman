package com.dx.agent2.thread;

import java.util.List;

import android.app.ActivityManager;
import android.content.Context;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.net.Uri;
import android.provider.Settings;
import android.support.v4.content.LocalBroadcastManager;

import com.dx.agent2.CrashApplication;
import com.dx.agent2.LogEmptyActivity;
import com.dx.service.ZMsg2;
import com.dx.utils.Log;
import com.dx.utils.SpUtil;
import com.dx.utils.ZThreadPool;
import com.dx.utils.ZThreadPool.ZThread;

public class ThreadStartApps extends ZThread {
    public static final String TAG = "ThreadStartApps";
    private Context mContext;
    private SpUtil spUtil;
    private ActivityManager am;
    private PackageManager pm;
    private LocalBroadcastManager lbm;
    private ZThreadPool threadPool;
    private boolean needsQuit = false;
    private List<String> pkgs;
    private int length = 0;
    private int index = 0;
    private String beforeLog, afterLog;

    public ThreadStartApps(Context context, ZThreadPool parent) {
        super(parent, TAG);
        this.mContext = context;
        this.am = (ActivityManager) mContext.getSystemService(Context.ACTIVITY_SERVICE);
        this.pm = mContext.getPackageManager();
        this.lbm = LocalBroadcastManager.getInstance(mContext);
        this.threadPool = CrashApplication.getInstance().getThreadPool();
        this.spUtil = SpUtil.getInstance(mContext);
        this.pkgs = spUtil.getPkgList(SpUtil.SP_NEEDS_START_APPS);
        this.length = setLength(pkgs.size());
        this.needsQuit = length == 0;
    }

    private void addLog(String log) {
        Intent intent = new Intent(ZMsg2.ZMSG2_BROADCAST_ACTION);
        intent.putExtra("cmd", ZMsg2.ZMSG2_CMD_ADD_LOG);
        intent.putExtra("time", System.currentTimeMillis());
        intent.putExtra("log", log);
        lbm.sendBroadcast(intent);
    }

    private int setLength(int size) {
        ActivityManager.MemoryInfo mi = new ActivityManager.MemoryInfo();
        am.getMemoryInfo(mi);
        long availMem = mi.availMem / 1048576;// MB
        int count = (int) (availMem / 70);
        beforeLog = "setLength(" + size + ") availMem=" + availMem + ", count=" + count;
        if (count >= 0 && count < 6) {
            count = 6;
        }
        if (count >= size) {
            count = size;
        }
        afterLog = "setLength(" + size + ") availMem=" + availMem + ", count=" + count;
        return count;
    }

    /**
     * 设置屏幕无操作休眠时间
     * 
     * @param minute
     *            分钟
     */
    private void setScreenOffTimeOut(int minute) {
        // 1分钟=60000毫秒；30分钟=1800000毫秒
        long time = minute * 60000;
        try {
            Settings.System.putLong(mContext.getContentResolver(), Settings.System.SCREEN_OFF_TIMEOUT, time);
            Log.saveLog2File(TAG, "setScreenOffTimeOut(" + minute + "minutes)", TAG);
        } catch (Exception e) {
            e.printStackTrace();
        }
    }

    @Override
    public void core_run() throws Exception {
        addLog("推广应用首次启动操作即将开始！");
        Log.saveLog2File(TAG, "before " + beforeLog, TAG);
        Log.saveLog2File(TAG, "after " + afterLog, TAG);
        setScreenOffTimeOut(30);

        try {
            Thread.sleep(5 * 1000);// 等待5秒
        } catch (Exception e) {
            e.printStackTrace();
        }

        while (!needsQuit) {
            try {
                if (index < length) {
                    String pkg = pkgs.get(index);
                    Intent it = pm.getLaunchIntentForPackage(pkg);
                    if (it != null) {
                        Log.saveLog2File(TAG, "start[" + pkg + "]succeed", TAG);
                        it.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
                        mContext.startActivity(it);
                    } else {
                        Log.saveLog2File(TAG, "start[" + pkg + "]failed", TAG);
                    }
                    index++;

                    if (index >= length) {
                        Log.saveLog2File(TAG, "10 seconds after, send finish log", TAG);
                    } else {
                        Log.saveLog2File(TAG, "10 seconds after, start next app", TAG);
                    }
                    Thread.sleep(10 * 1000);
                } else {
                    Intent itHome = new Intent(Intent.ACTION_MAIN);
                    itHome.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
                    itHome.addCategory(Intent.CATEGORY_HOME);
                    mContext.startActivity(itHome);
                    Thread.sleep(5 * 1000);

                    needsQuit = true;
                    index = 0;
                    if (spUtil.isYunOs()) {
                        Log.saveLog2File(TAG, "YunOS first start finish!", TAG);
                        Intent it = new Intent(mContext, LogEmptyActivity.class);
                        it.setData(Uri.parse("推广应用首次启动操作已经完成！"));
                        it.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
                        mContext.startActivity(it);
                    } else {
                        Log.saveLog2File(TAG, "AndroidOS first start finish!", TAG);

                        addLog("推广应用首次启动操作已经完成！");

                        Intent intent = new Intent(ZMsg2.ZMSG2_BROADCAST_ACTION);
                        intent.putExtra("cmd", ZMsg2.ZMSG2_CMD_SET_ALERT);
                        intent.putExtra("alertType", ZMsg2.ZMSG2_ALERT_FINISH_START);
                        lbm.sendBroadcast(intent);
                    }

                    spUtil.putLong(SpUtil.SP_LAST_START_FINISHED_TIME, System.currentTimeMillis());
                }
            } catch (Exception e) {
                index++;
                e.printStackTrace();
            }
        }

        if (!spUtil.isYunOs()) {
            // 置顶日志界面，直至出现为止
            new ThreadTopActivity(mContext, threadPool).start();
        }
        setScreenOffTimeOut(1);
    }

}
