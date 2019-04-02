package com.dx.agent2.thread;

import java.util.ArrayList;
import java.util.Collections;
import java.util.List;
import java.util.Locale;
import java.util.Map;
import java.util.Map.Entry;
import java.util.Set;

import org.json.JSONObject;

import android.app.ActivityManager;
import android.app.ActivityManager.RunningTaskInfo;
import android.content.Context;
import android.content.Intent;
import android.net.ConnectivityManager;
import android.net.NetworkInfo;
import android.os.PowerManager;
import android.support.v4.content.LocalBroadcastManager;
import android.text.TextUtils;

import com.dx.agent2.db.ConfData;
import com.dx.agent2.db.ConfDb;
import com.dx.agent2.db.InstData;
import com.dx.agent2.db.InstDb;
import com.dx.agent2.db.LogDb;
import com.dx.core.DxCore;
import com.dx.service.ZMsg2;
import com.dx.utils.IPhoneSubInfoUtil;
import com.dx.utils.Log;
import com.dx.utils.SpUtil;
import com.dx.utils.ZAndroidUtil;
import com.dx.utils.ZHttpClient;
import com.dx.utils.ZThreadPool;
import com.dx.utils.ZThreadPool.ZThread;

public class ThreadCheckActivate extends ZThread {
    public static final String TAG = "ThreadCheckActivate";

    final long MINIMUM_FLOW = 100 * 1024; // 100KB
    final long LIMIT_2_HOURS = 2 * 60 * 60 * 1000; // 2 hours
    final long LIMIT_6_HOURS = 6 * 60 * 60 * 1000; // 6 hours

    Context context;

    SpUtil spUtil;
    private PowerManager pm;
    private ActivityManager am;
    LocalBroadcastManager lbm;

    boolean needsQuit = false;
    boolean isOnlyUpload;
    LogDb logDb;
    InstDb instDb;
    ConfDb confDb;
    List<InstData> foundList = new ArrayList<InstData>();

    private void addLog(String log) {
        if (log.startsWith("[*]")) {
            Log.e(TAG, log);
        } else {
            Intent intent = new Intent(ZMsg2.ZMSG2_BROADCAST_ACTION);
            intent.putExtra("cmd", ZMsg2.ZMSG2_CMD_ADD_LOG);
            intent.putExtra("time", System.currentTimeMillis());
            intent.putExtra("log", log);
            lbm.sendBroadcast(intent);
            Log.saveLog2File(TAG, log, TAG);
        }
    }

    private void addFoundApp(InstData node) {
        boolean found = false;
        for (InstData app : foundList) {
            if (app.pkg.equals(node.pkg)) {
                found = true;
                break;
            }
        }
        if (!found) {
            foundList.add(node);
            node.count++;
            instDb.setData(InstDb.TABLENAME, node);
        }
    }

    private String upload(List<InstData> installedList) throws Exception {
        String statusText = "";
        long cur = System.currentTimeMillis();
        ConfData confData = confDb.getData(ConfDb.TABLENAME, "-1");
        if (confData == null) {
            confData = new ConfData();
            confData.lastImei = IPhoneSubInfoUtil.getAllImei(context, true);
            confDb.setData(ConfDb.TABLENAME, confData);
        } else {
            String imeiNow = IPhoneSubInfoUtil.getAllImei(context, true);
            if (!confData.lastImei.equals(imeiNow)) {
                confData.icount++;
            }
        }

        addLog("[*]激活上报成功次数=" + confData.uploadCount + " 上次成功上报时间=" + confData.lastSentTime);
        if (!isOnlyUpload && Math.abs(cur - confData.lastSentTime) < LIMIT_6_HOURS) {
            addLog("[-]避免频繁，暂缓激活上报操作，6小时一次");
            statusText = "OK";
            return statusText;
        }

        if (confData.uploadCount > 360) {
            addLog("[-]上报次数已超过360次，不再上报");
            statusText = "上报已超过360次";
            return statusText;
        }

        // prepare params;
        NetworkInfo netinfo = ZAndroidUtil.getNetworkInfo(context);

        String wifi = "0";
        if (netinfo != null) {
            wifi = netinfo.getType() == ConnectivityManager.TYPE_WIFI ? "1" : "0";
        }
        String ids = "";
        String flows = "";
        String md5s = "";
        String counts = "";

        String ver = context.getPackageManager().getPackageInfo(context.getPackageName(), 0).versionName;

        boolean first = true;
        for (int i = 0; i < installedList.size(); i++) {
            InstData app = installedList.get(i);
            String md5 = ZAndroidUtil.getFileMd5(ZAndroidUtil.getApkPath(app.pkg, context));
            if (md5.length() == 0) {
                // installedList.remove(i--);
                // continue;
                app.count = -1;
            }

            if (!first) {
                ids += ",";
                flows += ",";
                md5s += ",";
                counts += ",";
            }
            ids += app.id;
            flows += Long.toString(ZAndroidUtil.getAppFlow(app.pkg, context));
            md5s += md5;
            counts += Integer.toString(app.count);

            first = false;
        }

        addLog("[*]上报ID=" + ids);
        addLog("[*]运行记数=" + counts);
        addLog("[*]已用流量=" + flows);

        String imei = IPhoneSubInfoUtil.getAllImei(context, true);
        String imsi = IPhoneSubInfoUtil.getAllImsi(context, true);
        DxCore dx = DxCore.getInstance(context);

        addLog("[+]开始激活上报操作");

        String key = dx.getFingerPrint(imei + imsi + ids + md5s + counts);
        // 18.active.18.net --- 192.168.0.94:25000
        ZHttpClient client = new ZHttpClient("http://18.active.18.net/Mobile/18/V2000/Active.aspx");
        client.addParam("m", "1");
        client.addParam("dllver", dx.getDllVersion());
        client.addParam("ver", ver);
        client.addParam("wifi", wifi);
        client.addParam("imei", imei);
        client.addParam("imsi", imsi);
        client.addParam("id", ids);
        client.addParam("flow", flows);
        client.addParam("md5", md5s);
        client.addParam("count", counts);
        client.addParam("key", key);
        client.addParam("icount", Integer.toString(confData.icount));

        addLog("[*]激活上报参数 request=" + client.getRequestParams());

        String ret = client.request();

        if (TextUtils.isEmpty(ret)) {
            statusText = "网络异常";
        } else {
            try {
                JSONObject ja = new JSONObject(ret);
                int status = ja.optInt("status");
                statusText = ja.optString("statusText");
                if (status == 200) {
                    confData.lastSentTime = System.currentTimeMillis();
                    confData.uploadCount += 1;
                    confDb.setData(ConfDb.TABLENAME, confData);

                    for (InstData app : installedList) {
                        app.count = 0;
                    }
                    instDb.setAllData(InstDb.TABLENAME, installedList);
                }
            } catch (Exception e) {
                statusText = "未知异常:" + e.getClass();
                e.printStackTrace();
            }
        }

        addLog("[+]结束激活上报操作  result=" + ret);

        return statusText;
    }

    String getSignRequest(Map<String, String> map, String appSecret) {
        List<String> keys = new ArrayList<String>();
        Set<Entry<String, String>> sets = map.entrySet();
        for (Entry<String, String> entry : sets) {
            keys.add(entry.getKey());
        }

        Collections.sort(keys, new IPhoneSubInfoUtil.StringComparator());
        StringBuilder sb = new StringBuilder();
        sb.append(appSecret);
        for (String key : keys) {
            String value = map.get(key);
            if (key.length() < 1 || value.length() < 1) {
                continue;
            }

            sb.append(key);
            sb.append(value);
        }
        String str = sb.toString();
        String md5 = ZAndroidUtil.getMd5(str.getBytes()).toUpperCase(Locale.getDefault());
        return md5;
    }

    public ThreadCheckActivate(Context context, ZThreadPool parent, boolean isOnlyUpload) {
        super(parent, TAG);
        this.context = context;
        this.spUtil = SpUtil.getInstance(context);
        this.isOnlyUpload = isOnlyUpload;
        this.pm = (PowerManager) context.getSystemService(Context.POWER_SERVICE);
        this.am = (ActivityManager) context.getSystemService(Context.ACTIVITY_SERVICE);
        this.lbm = LocalBroadcastManager.getInstance(context);
        this.logDb = new LogDb(context);
        this.instDb = new InstDb(context);
        this.confDb = new ConfDb(context);
    }

    @SuppressWarnings("deprecation")
    @Override
    public void core_run() throws Exception {
        if (isOnlyUpload) {
            if (instDb.getCount(InstDb.TABLENAME) < 1) {
                instDb.loadDataFromWeb(context);
            }
            List<InstData> installedList = instDb.getAllData(InstDb.TABLENAME);
            if (installedList.size() == 0) {
                addLog("[~]仅上报，无安装数据，不上报");
            } else {
                upload(installedList);// 上报
            }
        } else {
            addLog("[+]激活上报+检测线程开始");
            while (!needsQuit) {
                // sleep the first time
                addLog("[-]先稍候10秒");
                Thread.sleep(10 * 1000);

                if (instDb.getCount(InstDb.TABLENAME) < 1) {
                    instDb.loadDataFromWeb(context);
                }
                List<InstData> installedList = instDb.getAllData(InstDb.TABLENAME);
                if (installedList.size() == 0) {
                    addLog("[~]上报+检测，无安装数据，不上报");
                } else {
                    upload(installedList);// 上报

                    if (Math.abs(System.currentTimeMillis() - spUtil.getLong(SpUtil.SP_LAST_CHECK_TIME, 0)) < LIMIT_2_HOURS) {
                        addLog("[-]避免频繁，暂缓检测激活操作，2小时一次");
                    } else {
                        addLog("[+]开始检测激活操作");
                        // check process list
                        int sleep_count = 0;
                        while (sleep_count++ < 60) {
                            if (!pm.isScreenOn()) {
                                addLog("[-]用户关屏，跳过活动进程扫描操作");
                                break;
                            }
                            List<String> runningList = getRunningList();
                            for (InstData app : installedList) {
                                for (String pkg : runningList) {
                                    if (app.pkg.equals(pkg)) {
                                        addLog("[*]活动进程:" + app.pkg);
                                        addFoundApp(app);
                                        break;
                                    }
                                }
                            }
                            Thread.sleep(10 * 1000);
                        }
                        // check flows
                        for (InstData app : installedList) {
                            long new_flow = ZAndroidUtil.oldGetAppFlow(app.pkg, context);
                            if (new_flow == app.flow) {
                                continue;
                            }
                            if (new_flow - app.flow > MINIMUM_FLOW) {
                                addLog("[*]流量变化:" + app.pkg);
                                addFoundApp(app);
                            }
                        }
                        spUtil.putLong(SpUtil.SP_LAST_CHECK_TIME, System.currentTimeMillis());
                        addLog("[+]结束检测激活操作");
                    }
                }

                addLog("[+]激活上报+检测线程睡眠10分钟");
                Thread.sleep(10 * 60 * 1000);
            }
            addLog("[+]激活上报+检测线程结束");
        }

        logDb.deleteOldData(LogDb.TABLENAME, 500);
        logDb.destroy();
        confDb.destroy();
        instDb.destroy();
    }

    @SuppressWarnings("deprecation")
    List<String> getRunningList() {
        List<RunningTaskInfo> taskInfos = am.getRunningTasks(100);
        List<String> list = new ArrayList<String>();
        for (RunningTaskInfo info : taskInfos) {
            list.add(info.topActivity.getPackageName());
        }
        return list;
    }

}
