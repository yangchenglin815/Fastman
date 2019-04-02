package com.dx.agent2.thread;

import android.content.Context;
import android.content.Intent;
import android.net.wifi.WifiManager;
import android.support.v4.content.LocalBroadcastManager;
import android.text.TextUtils;

import com.dx.agent2.CrashApplication;
import com.dx.consts.Const;
import com.dx.service.ZMsg2;
import com.dx.utils.IPhoneSubInfoUtil;
import com.dx.utils.SpUtil;
import com.dx.utils.ZHttpClient;
import com.dx.utils.ZThreadPool;
import com.dx.utils.ZThreadPool.ZThread;

/**
 * 安装记录上报
 * 
 * @author guozg
 *
 */
public class ThreadInstallReport extends ZThread {
    public static final String TAG = "ThreadInstallReport";

    Context mContext;
    SpUtil spUtil;
    String url;
    LocalBroadcastManager lbm;
    WifiManager wm;
    String apkIds, newVersions, clientVersion;

    public ThreadInstallReport(Context context, ZThreadPool parent) {
        super(parent, TAG);
        this.mContext = context;
        this.spUtil = SpUtil.getInstance(mContext);
        this.url = spUtil.getString(SpUtil.SP_ASYNC_UPLOAD_URL, "");
        this.lbm = LocalBroadcastManager.getInstance(mContext);
        this.wm = (WifiManager) mContext.getSystemService(Context.WIFI_SERVICE);
        this.apkIds = spUtil.getString(SpUtil.SP_ASYNC_UPLOAD_LOW_APKIDS, "");
        this.newVersions = spUtil.getString(SpUtil.SP_ASYNC_UPLOAD_LOW_NEWVERSIONS, "");
        this.clientVersion = spUtil.getString(SpUtil.SP_ASYNC_UPLOAD_LOW_CLIENTVERSION, "");
    }

    private void addLog(String log) {
        Intent intent = new Intent(ZMsg2.ZMSG2_BROADCAST_ACTION);
        intent.putExtra("cmd", ZMsg2.ZMSG2_CMD_ADD_LOG);
        intent.putExtra("time", System.currentTimeMillis());
        intent.putExtra("log", log);
        lbm.sendBroadcast(intent);
    }

    private void addLogAndAlert(String log, short alert_type) {
        addLog(log);
        Intent intent = new Intent(ZMsg2.ZMSG2_BROADCAST_ACTION);
        intent.putExtra("cmd", ZMsg2.ZMSG2_CMD_SET_ALERT);
        intent.putExtra("alertType", alert_type);
        lbm.sendBroadcast(intent);
    }

    private String report(String url) throws Exception {
        String statusText = "";
        if (TextUtils.isEmpty(url)) {
            statusText = "URL为空";
        } else {
            String urlImei = url.substring(url.indexOf("&imei=") + 6, url.indexOf("&pwd="));
            if (TextUtils.isEmpty(urlImei)) {
                addLog("UrlImei为空");
            } else {
                urlImei = urlImei.replaceAll("%2c", ",");
                addLog("UrlImei:" + urlImei);
                String phoneImei = IPhoneSubInfoUtil.getAllImei(mContext, true);
                if (!phoneImei.equalsIgnoreCase(urlImei)) {
                    addLog("PhoneImei:" + phoneImei);
                    url = Const.updateUrl(phoneImei, urlImei, url);
                    spUtil.putString(SpUtil.SP_ASYNC_UPLOAD_URL, url);
                }
            }
            ZHttpClient client = new ZHttpClient(url);
            statusText = client.get();
        }
        return statusText;
    }

    @Override
    public void core_run() throws Exception {
        addLog("再次上报安装记录...");

        String lowResult = spUtil.getString(SpUtil.SP_ASYNC_UPLOAD_LOW_RESULT, "");
        if (!lowResult.startsWith("OK")) {
            lowResult = ZHttpClient.installFailedReport(apkIds, newVersions, clientVersion);
            spUtil.putString(SpUtil.SP_ASYNC_UPLOAD_LOW_RESULT, lowResult);
            addLog("失败记录重新上报结果：" + lowResult);
        }

        String result = report(url);// 上报
        spUtil.putString(SpUtil.SP_UPLOAD_RESULT, result);

        if (result.startsWith("OK")) {
            wm.setWifiEnabled(false);
            addLogAndAlert("安装记录重新上报成功：" + result, ZMsg2.ZMSG2_ALERT_ASYNC_REPORT_SUCCEED);
            new ThreadStartApps(mContext, CrashApplication.getInstance().getThreadPool()).start();
        } else {
            addLogAndAlert("安装记录重新上报失败：" + result, ZMsg2.ZMSG2_ALERT_ASYNC_REPORT_FAILED);
        }
    }

}
