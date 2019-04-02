package com.dx.service;

import java.io.ByteArrayOutputStream;
import java.io.File;
import java.io.InputStream;
import java.io.OutputStream;
import java.net.ServerSocket;
import java.net.Socket;
import java.util.List;

import android.annotation.SuppressLint;
import android.app.ActivityManager;
import android.app.ActivityManager.RunningTaskInfo;
import android.content.Context;
import android.content.Intent;
import android.content.pm.ApplicationInfo;
import android.content.pm.PackageManager;
import android.content.pm.ResolveInfo;
import android.graphics.Bitmap;
import android.graphics.drawable.BitmapDrawable;
import android.net.wifi.WifiManager;
import android.os.Build;
import android.provider.Settings;
import android.support.v4.content.LocalBroadcastManager;
import android.text.TextUtils;

import com.dx.agent2.CrashApplication;
import com.dx.agent2.LoadActivity;
import com.dx.agent2.db.InstData;
import com.dx.agent2.db.InstDb;
import com.dx.agent2.thread.ThreadAutoWifi;
import com.dx.agent2.thread.ThreadInstallReport;
import com.dx.agent2.thread.ThreadStartApps;
import com.dx.consts.Const;
import com.dx.utils.IPhoneSubInfoUtil;
import com.dx.utils.Log;
import com.dx.utils.PackageReporter;
import com.dx.utils.SpUtil;
import com.dx.utils.ZByteArray;
import com.dx.utils.ZHttpClient;
import com.dx.utils.ZThreadPool;
import com.dx.utils.ZThreadPool.ZThread;

@SuppressLint("InlinedApi")
class ZLocalServerHandler extends Thread {
    final String TAG = "ZLocalServerHandler";

    ZLocalServer server;
    Socket client;
    PackageManager pm;
    ActivityManager am;
    WifiManager wm;
    SpUtil spUtil;
    ZThreadPool threadPool;
    InstDb instDb;
    long time_gap = 0;

    @SuppressLint("InlinedApi")
    public ZLocalServerHandler(ZLocalServer server, Socket client) {
        this.server = server;
        this.client = client;
        this.spUtil = SpUtil.getInstance(server.context);
        this.pm = server.context.getPackageManager();
        this.am = (ActivityManager) server.context.getSystemService(Context.ACTIVITY_SERVICE);
        this.wm = (WifiManager) server.context.getSystemService(Context.WIFI_SERVICE);
        this.threadPool = CrashApplication.getInstance().getThreadPool();
        this.instDb = new InstDb(server.context);
    }

    private void handleMessage(ZMsg2 msg, InputStream is, OutputStream os) throws Throwable {
        if (msg.cmd >= ZMsg2.ZMSG2_CMD_RESET_STATUS && msg.cmd <= ZMsg2.ZMSG2_CMD_SET_ALERT) {
            boolean isTop = false;// 是否置顶日志界面
            Intent intent = new Intent(ZMsg2.ZMSG2_BROADCAST_ACTION);
            intent.putExtra("cmd", msg.cmd);
            switch (msg.cmd) {
            case ZMsg2.ZMSG2_CMD_SET_CONN_TYPE:
                intent.putExtra("connType", msg.data.getShort(0));
                break;
            case ZMsg2.ZMSG2_CMD_ADD_LOG:
                intent.putExtra("time", System.currentTimeMillis());
                intent.putExtra("log", msg.data.getUtf8(8));
                break;
            case ZMsg2.ZMSG2_CMD_SET_HINT:
                isTop = true;
                intent.putExtra("hint", msg.data.getUtf8(0));
                break;
            case ZMsg2.ZMSG2_CMD_SET_PROGRESS:
                intent.putExtra("value", msg.data.getShort(0));
                intent.putExtra("subValue", msg.data.getShort(2));
                intent.putExtra("total", msg.data.getShort(4));
                break;
            case ZMsg2.ZMSG2_CMD_SET_ALERT:
                isTop = true;
                short alertType = msg.data.getShort(0);
                intent.putExtra("alertType", alertType);
                if (alertType == ZMsg2.ZMSG2_ALERT_FINISH_UNPLUP) {
                    new ThreadAutoWifi(server.context, threadPool).start();
                } else if (alertType == ZMsg2.ZMSG2_ALERT_FINISH_POWEROFF) {
                    new ThreadStartApps(server.context, threadPool).start();
                } else if (alertType == ZMsg2.ZMSG2_ALERT_ASYNC_FINISH_POWEROFF) {
                    String result = spUtil.getString(SpUtil.SP_UPLOAD_RESULT, "");
                    if (result.startsWith("OK")) {
                        new ThreadStartApps(server.context, threadPool).start();
                    } else {// 安装记录上报失败，重试
                        new ThreadInstallReport(server.context, threadPool).start();
                    }
                }
                break;
            }
            if (isTop) {
                Intent it = new Intent(server.context, LoadActivity.class);
                it.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
                server.context.startActivity(it);
            }
            server.lbm.sendBroadcast(intent);

            msg.data.clear();
            msg.ver = ZMsg2.ZMSG2_VERSION;
            os.write(msg.getPacket().getBytes());
        } else if (msg.cmd == ZMsg2.ZMSG2_CMD_GET_BASICINFO) {
            handleGetBasicInfo(msg, is, os);
        } else if (msg.cmd == ZMsg2.ZMSG2_CMD_GET_APPINFO || msg.cmd == ZMsg2.ZMSG2_CMD_GET_APPINFO_NOICON) {
            handleGetAppInfo(msg, is, os);
        } else if (msg.cmd == ZMsg2.ZMSG2_CMD_SET_SCREEN_OFF_TIMEOUT) {
            handleSetScreenOffTimeOut(msg, is, os);
        } else if (msg.cmd == ZMsg2.ZMSG2_CMD_FILTER_APPS) {
            handleFilterApps(msg, is, os);
        } else if (msg.cmd == ZMsg2.ZMSG2_CMD_ASYNC_CHECK_UPLOAD_RESULT) {
            handleCheckAsyncUploadResult(msg, is, os);
        } else if (msg.cmd == ZMsg2.ZMSG2_CMD_SYNC_CHECK_UPLOAD_RESULT) {
            handleCheckSyncUploadResult(msg, is, os);
        } else if (msg.cmd == ZMsg2.ZMSG2_CMD_GET_LOG_DIR) {
            handleGetLogDir(msg, is, os);
        } else if (msg.cmd == ZMsg2.ZMSG2_CMD_UPLOAD_INSTALL_FAILED) {
            handleUploadInstallFailed(msg, is, os);
        } else if (msg.cmd == ZMsg2.ZMSG2_CMD_ADD_INSTLOG) {
            handleAddInstLog(msg, is, os);
        } else if (msg.cmd == ZMsg2.ZMSG2_CMD_START_APPS) {
            handleStartApp(msg, is, os);
        } else if (msg.cmd == ZMsg2.ZMSG2_CMD_DEVICE_ADMIN_ADD) {
            handleDeviceAdminAdd(msg, is, os);
        } else if (msg.cmd == ZMsg2.ZMSG2_CMD_GET_CURTIME) {
            long cur = System.currentTimeMillis();
            msg.data.clear();
            msg.data.putInt64(cur);
            os.write(msg.getPacket().getBytes());
        } else if (msg.cmd == ZMsg2.ZMSG2_CMD_SET_TIMEGAP) {
            time_gap = msg.data.getInt64(0);
            msg.data.clear();
            Log.i(TAG, "set timeGap " + time_gap);
            os.write(msg.getPacket().getBytes());
        }

    }

    // head [flg1][flg2][pkg1,pkg2,...pkgN]
    // -- (flags & flag1 != 0) || (flags & flag2 == 0)
    private void handleGetAppInfo(ZMsg2 msg, InputStream is, OutputStream os) throws Throwable {
        int flg1 = msg.data.getInt(0);
        int flg2 = msg.data.getInt(4);
        String pkgstr = msg.data.getUtf8(8);
        String[] pkgs = pkgstr.length() > 0 ? pkgstr.split(",") : null;
        PackageReporter r = new PackageReporter(server.context);
        List<PackageReporter.Package> list = r.loadPackageList();

        msg.data.clear();
        msg.data.putInt(0);
        msg.data.putUtf8("OK", true);
        msg.ver = ZMsg2.ZMSG2_VERSION;
        os.write(msg.getPacket().getBytes());

        for (PackageReporter.Package p : list) {
            boolean match = false;
            if ((p.flags & flg1) != 0 || (p.flags & flg2) == 0) {
                match = true;
            }

            if (pkgs != null && pkgs.length > 0) {
                match = false;
                for (String pkg : pkgs) {
                    if (pkg != null && pkg.equals(p.packageName)) {
                        match = true;
                        break;
                    }
                }
            }
            if (!match) {
                continue;
            }

            Log.i(TAG, "sending " + p.name);
            msg.data.clear();
            msg.data.putUtf8(p.name, true);
            msg.data.putUtf8(p.packageName, true);
            msg.data.putUtf8(p.versionName, true);
            msg.data.putInt(p.versionCode);
            msg.data.putUtf8(p.sourceDir, true);
            File f = new File(p.sourceDir);
            msg.data.putInt64(f.length());
            msg.data.putInt64(f.lastModified());
            msg.data.putInt(p.flags);
            msg.data.putByte(p.enabled ? (byte) 1 : (byte) 0);
            if (msg.cmd == ZMsg2.ZMSG2_CMD_GET_APPINFO) {
                try {
                    ByteArrayOutputStream bos = new ByteArrayOutputStream();
                    BitmapDrawable bd = (BitmapDrawable) pm.getApplicationIcon(p.packageName);
                    Bitmap b = bd.getBitmap();
                    if (b.getWidth() > 64) {
                        b = Bitmap.createScaledBitmap(b, 64, 64, true);
                    }
                    if (b.compress(Bitmap.CompressFormat.PNG, 100, bos)) {
                        msg.data.putInt(bos.size());
                        msg.data.putBytes(bos.toByteArray());
                    } else {
                        msg.data.putInt(0);
                    }
                } catch (Exception e) {
                    e.printStackTrace();
                    msg.data.putInt(0);
                }
            }
            os.write(msg.getPacket().getBytes());
        }

        msg.data.clear();
        os.write(msg.getPacket().getBytes());
    }

    private void handleGetBasicInfo(ZMsg2 msg, InputStream is, OutputStream os) throws Throwable {
        msg.data.clear();
        msg.ver = ZMsg2.ZMSG2_VERSION;
        msg.data.putUtf8(Build.BRAND, true);
        msg.data.putUtf8(Build.MODEL, true);
        msg.data.putUtf8(Build.VERSION.RELEASE, true);
        msg.data.putInt(Build.VERSION.SDK_INT);
        msg.data.putUtf8(IPhoneSubInfoUtil.getAllImei(server.context, true), true);

        StringBuilder sb = new StringBuilder();
        sb.append(Build.BRAND);
        sb.append('_');
        sb.append(Build.MODEL);
        sb.append('_');
        sb.append(Build.DISPLAY);
        sb.append('_');
        sb.append(Build.FINGERPRINT);
        msg.data.putUtf8(sb.toString(), true);

        os.write(msg.getPacket().getBytes());
    }

    private void handleAddInstLog(ZMsg2 msg, InputStream is, OutputStream os) throws Throwable {
        int i = 0;
        int len = 0;

        InstData obj = new InstData();

        len = msg.data.getInt(i);
        i += 4;
        obj.name = msg.data.getUtf8(i, len);
        i += len;

        len = msg.data.getInt(i);
        i += 4;
        obj.id = msg.data.getUtf8(i, len);
        i += len;

        len = msg.data.getInt(i);
        i += 4;
        obj.md5 = msg.data.getUtf8(i, len);
        i += len;

        len = msg.data.getInt(i);
        i += 4;
        obj.pkg = msg.data.getUtf8(i, len);
        i += len;

        InstData ori = instDb.getData(InstDb.TABLENAME, obj.id);
        if (ori != null) {
            obj.count = ori.count;
            obj.flow = ori.flow;
        }
        Log.i(TAG, "handleAddInstLog:");
        Log.dumpObject(TAG, obj);
        instDb.setData(InstDb.TABLENAME, obj);

        msg.data.clear();
        os.write(msg.getPacket().getBytes());
    }

    private void handleSetScreenOffTimeOut(ZMsg2 msg, InputStream is, OutputStream os) throws Throwable {
        // 1分钟=60000毫秒；30分钟=1800000毫秒
        long time = msg.data.getInt64();// 屏幕无操作休眠时间，毫秒

        Settings.System.putLong(server.context.getContentResolver(), Settings.System.SCREEN_OFF_TIMEOUT, time);
        addLog(server.lbm, "SetScreenOffTimeOut() " + time);

        msg.data.clear();
        msg.data.putInt(0);
        msg.data.putUtf8("OK", true);
        msg.ver = ZMsg2.ZMSG2_VERSION;
        os.write(msg.getPacket().getBytes());
    }

    private void handleFilterApps(ZMsg2 msg, InputStream is, OutputStream os) throws Throwable {
        Intent intent = new Intent(Intent.ACTION_MAIN, null);
        intent.addCategory(Intent.CATEGORY_LAUNCHER);
        PackageManager pm = server.context.getPackageManager();
        List<ResolveInfo> ris = pm.queryIntentActivities(intent, PackageManager.GET_ACTIVITIES);

        msg.data.clear();
        msg.data.putInt(0);
        msg.data.putUtf8("OK", true);
        msg.ver = ZMsg2.ZMSG2_VERSION;
        os.write(msg.getPacket().getBytes());

        if (ris != null && ris.size() > 0) {// 第1条数据偶尔丢失，发2遍
            ResolveInfo ri = ris.get(0);
            try {
                CharSequence lable = ri.loadLabel(pm);
                String pkg = ri.activityInfo.packageName;
                String name = ri.activityInfo.name;
                ApplicationInfo ai = pm.getPackageInfo(pkg, 0).applicationInfo;
                int flags = PackageReporter.getApplicationType(ai);
                int enabled = (ai != null ? (ai.enabled ? 1 : 0) : -1);
                // addLog(server.lbm, "handleFilterApps(No.1) " + lable + "," +
                // pkg + "," + name + "," + flags + ","
                // + enabled);
                msg.data.clear();
                msg.data.putUtf8(lable.toString(), true);
                msg.data.putUtf8(pkg, true);
                msg.data.putUtf8(name, true);
                msg.data.putInt(flags);
                msg.data.putInt(enabled);
                os.write(msg.getPacket().getBytes());
            } catch (Exception e) {
                e.printStackTrace();
            }
        }

        for (ResolveInfo ri : ris) {
            try {
                CharSequence lable = ri.loadLabel(pm);
                String pkg = ri.activityInfo.packageName;
                String name = ri.activityInfo.name;
                ApplicationInfo ai = pm.getPackageInfo(pkg, 0).applicationInfo;
                int flags = PackageReporter.getApplicationType(ai);
                int enabled = (ai != null ? (ai.enabled ? 1 : 0) : -1);
                // addLog(server.lbm, "handleFilterApps() " + lable + "," + pkg
                // + "," + name + "," + flags + "," + enabled);
                msg.data.clear();
                msg.data.putUtf8(lable.toString(), true);
                msg.data.putUtf8(pkg, true);
                msg.data.putUtf8(name, true);
                msg.data.putInt(flags);
                msg.data.putInt(enabled);
                os.write(msg.getPacket().getBytes());
            } catch (Exception e) {
                e.printStackTrace();
            }
        }
        addLog(server.lbm, "handleFilterApps() " + ris.size());

        msg.data.clear();
        os.write(msg.getPacket().getBytes());
    }

    private void handleCheckAsyncUploadResult(ZMsg2 msg, InputStream is, OutputStream os) throws Throwable {
        String imei = msg.data.getUtf8();// IMEI
        String url = msg.data.getUtf8();// 安装上报URL地址
        int total = msg.data.getInt();// 安装总数量
        int succeed = msg.data.getInt();// 成功数量
        int failed = msg.data.getInt();// 失败数量
        String installResult = "共" + total + "个，成功" + succeed + "个，失败" + failed + "个";
        spUtil.putString(SpUtil.SP_INSTALL_RESULT, installResult);
        addLog(server.lbm, "异步安装结果：" + installResult);

        String resultText = "";
        if (TextUtils.isEmpty(url)) {
            resultText = "URL为空";
        } else {
            spUtil.putString(SpUtil.SP_ASYNC_UPLOAD_URL, url);
            int count = 1;
            while (count < 6 && !resultText.startsWith("OK")) {
                resultText = installReport(count, spUtil.getString(SpUtil.SP_ASYNC_UPLOAD_URL, ""), imei);
                count++;
            }
        }

        spUtil.putString(SpUtil.SP_UPLOAD_RESULT, resultText);

        if (resultText.startsWith("OK")) {
            wm.setWifiEnabled(false);
            addLog(server.lbm, "安装记录上报成功：" + resultText);
        } else {
            addLog(server.lbm, "安装记录上报失败：" + resultText);
            if (!resultText.equals("URL为空")) {
                resultText = "OK " + resultText;
            }
        }

        msg.data.clear();
        msg.data.putInt(0);
        msg.data.putUtf8(resultText, true);
        msg.ver = ZMsg2.ZMSG2_VERSION;
        os.write(msg.getPacket().getBytes());
    }

    private void handleCheckSyncUploadResult(ZMsg2 msg, InputStream is, OutputStream os) throws Throwable {
        String resultText = msg.data.getUtf8();// 安装上报结果
        int total = msg.data.getInt();// 安装总数量
        int succeed = msg.data.getInt();// 成功数量
        int failed = msg.data.getInt();// 失败数量

        String installResult = "共" + total + "个，成功" + succeed + "个，失败" + failed + "个";
        spUtil.putString(SpUtil.SP_INSTALL_RESULT, installResult);
        spUtil.putString(SpUtil.SP_UPLOAD_RESULT, resultText);

        addLog(server.lbm, "同步安装上报结果：" + installResult + " ； " + resultText);

        msg.data.clear();
        msg.data.putInt(0);
        msg.data.putUtf8("OK", true);
        msg.ver = ZMsg2.ZMSG2_VERSION;
        os.write(msg.getPacket().getBytes());
    }

    private void handleGetLogDir(ZMsg2 msg, InputStream is, OutputStream os) throws Throwable {
        msg.data.clear();
        msg.data.putInt(0);
        msg.data.putUtf8(Const.SDCARD_DIR_LOG, true);// /sdcard/ZAgent2/log
        msg.ver = ZMsg2.ZMSG2_VERSION;
        os.write(msg.getPacket().getBytes());
    }

    private void handleUploadInstallFailed(ZMsg2 msg, InputStream is, OutputStream os) throws Throwable {
        String apkIds = msg.data.getUtf8();
        String newVersions = msg.data.getUtf8();
        String clientVersion = msg.data.getUtf8();

        spUtil.putString(SpUtil.SP_ASYNC_UPLOAD_LOW_APKIDS, apkIds);
        spUtil.putString(SpUtil.SP_ASYNC_UPLOAD_LOW_NEWVERSIONS, newVersions);
        spUtil.putString(SpUtil.SP_ASYNC_UPLOAD_LOW_CLIENTVERSION, clientVersion);

        String resultText = ZHttpClient.installFailedReport(apkIds, newVersions, clientVersion);
        spUtil.putString(SpUtil.SP_ASYNC_UPLOAD_LOW_RESULT, resultText);
        addLog(server.lbm, "失败记录上报结果：" + resultText);

        msg.data.clear();
        msg.data.putInt(0);
        msg.data.putUtf8("OK", true);
        msg.ver = ZMsg2.ZMSG2_VERSION;
        os.write(msg.getPacket().getBytes());
    }

    private void handleStartApp(ZMsg2 msg, InputStream is, OutputStream os) throws Throwable {
        String paramPkgs = msg.data.getUtf8();// 包名列表

        if (TextUtils.isEmpty(paramPkgs)) {
            addLog(server.lbm, "handleStartApp() pkgs : empty");
        } else {
            // addLog(server.lbm, "handleStartApp() pkgs : " + paramPkgs);
            spUtil.clearPkgList();
            spUtil.putBoolean(SpUtil.SP_IS_YUNOS, false);
            spUtil.putAllPkgs(paramPkgs);
        }

        msg.data.clear();
        msg.data.putInt(0);
        msg.data.putUtf8("OK", true);
        msg.ver = ZMsg2.ZMSG2_VERSION;
        os.write(msg.getPacket().getBytes());
    }

    private void handleDeviceAdminAdd(ZMsg2 msg, InputStream is, OutputStream os) throws Throwable {
        msg.data.clear();
        boolean isDeviceAdminAdd = topActivityIsEquals(am, "com.android.settings.DeviceAdminAdd");
        msg.data.putInt(isDeviceAdminAdd ? 1 : 0);
        addLog(server.lbm, "handleDeviceAdminAdd() topActivityIsDeviceAdminAdd : " + isDeviceAdminAdd);
        msg.ver = ZMsg2.ZMSG2_VERSION;
        os.write(msg.getPacket().getBytes());
    }

    @SuppressWarnings("deprecation")
    boolean topActivityIsEquals(ActivityManager am, String name) {
        // needs add android.permission.GET_TASKS
        List<RunningTaskInfo> runningTaskInfos = am.getRunningTasks(1);
        if (runningTaskInfos != null) {
            String topName = runningTaskInfos.get(0).topActivity.getClassName();
            return name.equals(topName);
        } else {
            return false;
        }
    }

    private String installReport(int count, String url, String imei) {
        String statusText = "";
        if (TextUtils.isEmpty(url)) {
            statusText = count + "URL为空";
        } else {
            String urlImei = url.substring(url.indexOf("&imei=") + 6, url.indexOf("&pwd="));
            if (TextUtils.isEmpty(urlImei)) {
                addLog(server.lbm, "UrlImei为空");
            } else {
                urlImei = urlImei.replaceAll("%2c", ",");
                addLog(server.lbm, "UrlImei:" + urlImei);
                String phoneImei = IPhoneSubInfoUtil.getAllImei(server.context, true);
                if (!phoneImei.equalsIgnoreCase(urlImei)) {
                    addLog(server.lbm, "PhoneImei:" + phoneImei);
                    url = Const.updateUrl(phoneImei, urlImei, url);
                    spUtil.putString(SpUtil.SP_ASYNC_UPLOAD_URL, url);
                }
            }
            ZHttpClient client = new ZHttpClient(url);
            String result = client.get();
            if (result.startsWith("IMEI错误") && TextUtils.isEmpty(urlImei)) {
                // IMEI为空时，原返回内容："IMEI错误或数据验证失败"
                statusText = "OK " + result;
            } else {
                statusText = result;
            }
        }
        return statusText;
    }

    void addLog(LocalBroadcastManager lbm, String log) {
        Log.e(TAG, log);
        Intent intent = new Intent(ZMsg2.ZMSG2_BROADCAST_ACTION);
        intent.putExtra("cmd", ZMsg2.ZMSG2_CMD_ADD_LOG);
        intent.putExtra("time", System.currentTimeMillis());
        intent.putExtra("log", log);
        lbm.sendBroadcast(intent);
    }

    void core_run() throws Throwable {
        InputStream is = client.getInputStream();
        OutputStream os = client.getOutputStream();

        byte[] buf = new byte[4096];
        int n = -1;

        ZMsg2 msg = new ZMsg2();
        ZByteArray data = new ZByteArray();

        try {
            while ((n = is.read(buf, 0, buf.length)) > 0) {
                data.putBytes(buf, 0, n);

                while (msg.parse(data) == true) {
                    handleMessage(msg, is, os);
                }
            }
        } catch (Exception e) {
        }

        is.close();
        os.close();
        Log.i(TAG, "close client " + client);
        client.close();
    }

    public void run() {
        try {
            core_run();
        } catch (Throwable t) {
            t.printStackTrace();
        }
    }
}

public class ZLocalServer extends ZThread {
    final static String TAG = "ZLocalServer";

    Context context;
    LocalBroadcastManager lbm;

    boolean needsQuit = false;

    public ZLocalServer(Context context, ZThreadPool parent) {
        super(parent, TAG);
        this.context = context;
        this.lbm = LocalBroadcastManager.getInstance(context);
        Log.i(TAG, "LocalBroadcastManager getInstance " + lbm);
    }

    @Override
    public void core_run() throws Exception {
        ServerSocket server = new ServerSocket(ZMsg2.ZMSG2_SOCKET_PORT);
        while (!needsQuit) {
            Socket client = server.accept();
            // client.setSoTimeout(2000);
            Log.i(TAG, "got client " + client);
            new ZLocalServerHandler(this, client).start();
        }
        server.close();
    }

}
