package com.dx.agent2.thread;

import java.io.BufferedReader;
import java.io.File;
import java.io.FileInputStream;
import java.io.InputStreamReader;

import android.content.Context;
import android.content.Intent;
import android.support.v4.content.LocalBroadcastManager;

import com.dx.agent2.CrashApplication;
import com.dx.service.ZMsg2;
import com.dx.utils.Log;
import com.dx.utils.SpUtil;
import com.dx.utils.ZHttpClient;
import com.dx.utils.ZThreadPool;
import com.dx.utils.ZThreadPool.ZThread;

public class ThreadCheckUrl extends ZThread {
    public static final String TAG = "ThreadCheckUrl";

    private Context mContext;
    private SpUtil spUtil;
    private LocalBroadcastManager lbm;
    private ZThreadPool threadPool;
    private boolean needsQuit = false;

    public ThreadCheckUrl(Context context, ZThreadPool parent) {
        super(parent, TAG);
        this.mContext = context;
        this.spUtil = SpUtil.getInstance(mContext);
        this.lbm = LocalBroadcastManager.getInstance(mContext);
        this.threadPool = CrashApplication.getInstance().getThreadPool();
    }

    private void addLog(String log) {
        Intent intent = new Intent(ZMsg2.ZMSG2_BROADCAST_ACTION);
        intent.putExtra("cmd", ZMsg2.ZMSG2_CMD_ADD_LOG);
        intent.putExtra("time", System.currentTimeMillis());
        intent.putExtra("log", log);
        lbm.sendBroadcast(intent);
    }

    private void setProgress(short value, short subValue, short total) {
        Intent intent = new Intent(ZMsg2.ZMSG2_BROADCAST_ACTION);
        intent.putExtra("cmd", ZMsg2.ZMSG2_CMD_SET_PROGRESS);
        intent.putExtra("value", value);
        intent.putExtra("subValue", subValue);
        intent.putExtra("total", total);
        lbm.sendBroadcast(intent);
    }

    @Override
    public void core_run() throws Exception {
        while (!needsQuit) {
            File file = new File("/data/local/tmp/work/zm_upload.log");
            if (file.isFile() && file.exists()) {
                try {
                    StringBuffer sb = new StringBuffer();
                    InputStreamReader read = new InputStreamReader(new FileInputStream(file), "UTF-8");
                    BufferedReader br = new BufferedReader(read);
                    String line = null;
                    while ((line = br.readLine()) != null) {
                        sb.append(line);
                    }
                    read.close();

                    String content = sb.toString();
                    String[] contents = content.split(";");
                    spUtil.putString(SpUtil.SP_ASYNC_UPLOAD_URL, contents[0]);
                    String installResult = "共" + contents[1] + "个，成功" + contents[2] + "个，失败" + contents[3] + "个";
                    spUtil.putString(SpUtil.SP_INSTALL_RESULT, installResult);

                    String result = spUtil.getString(SpUtil.SP_UPLOAD_RESULT, "");

                    String apkIds = "", newVersions = "", clientVersion = "";
                    if (contents.length > 4) {
                        apkIds = contents[4];
                        newVersions = contents[5];
                        clientVersion = contents[6];
                        spUtil.putString(SpUtil.SP_ASYNC_UPLOAD_LOW_APKIDS, apkIds);
                        spUtil.putString(SpUtil.SP_ASYNC_UPLOAD_LOW_NEWVERSIONS, newVersions);
                        spUtil.putString(SpUtil.SP_ASYNC_UPLOAD_LOW_CLIENTVERSION, clientVersion);
                        Log.e(TAG, apkIds + ";" + newVersions + ";" + clientVersion);
                    }

                    addLog("find zm_upload.log file");
                    setProgress((short) 10, (short) -1, (short) 10);

                    if (result.startsWith("OK")) {
                        String lowResult = spUtil.getString(SpUtil.SP_ASYNC_UPLOAD_LOW_RESULT, "");
                        if (!lowResult.startsWith("OK") && contents.length > 4) {
                            lowResult = ZHttpClient.installFailedReport(apkIds, newVersions, clientVersion);
                            spUtil.putString(SpUtil.SP_ASYNC_UPLOAD_LOW_RESULT, lowResult);
                            addLog("失败记录再次上报结果：" + lowResult);
                        }
                        if (!threadPool.hasThread(ThreadStartApps.TAG)) {
                            new ThreadStartApps(mContext, threadPool).start();
                        }
                    } else {// 安装记录上报失败，重试
                        new ThreadInstallReport(mContext, threadPool).start();
                    }

                    needsQuit = true;
                } catch (Exception e) {
                }
            } else {
                file = null;
                Thread.sleep(30 * 1000);
            }
        }
    }

}
