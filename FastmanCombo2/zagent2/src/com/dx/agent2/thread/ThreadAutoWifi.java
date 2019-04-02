package com.dx.agent2.thread;

import java.io.BufferedReader;
import java.io.File;
import java.io.FileInputStream;
import java.io.InputStreamReader;
import java.net.HttpURLConnection;
import java.net.URL;
import java.util.ArrayList;
import java.util.List;

import android.content.Context;
import android.content.Intent;
import android.net.ConnectivityManager;
import android.net.NetworkInfo;
import android.net.wifi.ScanResult;
import android.net.wifi.WifiConfiguration;
import android.net.wifi.WifiManager;
import android.support.v4.content.LocalBroadcastManager;
import android.text.TextUtils;
import android.util.Log;

import com.dx.service.ZMsg2;
import com.dx.utils.ZThreadPool;
import com.dx.utils.ZThreadPool.ZThread;

public class ThreadAutoWifi extends ZThread {
    public static final String TAG = "ThreadAutoWifi";

    private Context mContext;
    private WifiManager wm;
    private ConnectivityManager cm;
    private LocalBroadcastManager lbm;
    private String configName = "", configPwd = "";
    private List<WifiConfiguration> mWifiList = null;
    private int index = 0, length = 0, count = 0;

    private boolean needsQuit = false;

    public ThreadAutoWifi(Context context, ZThreadPool parent) {
        super(parent, TAG);
        this.mContext = context;
        this.lbm = LocalBroadcastManager.getInstance(mContext);
        this.wm = (WifiManager) mContext.getSystemService(Context.WIFI_SERVICE);
        this.cm = (ConnectivityManager) mContext.getSystemService(Context.CONNECTIVITY_SERVICE);
        this.mWifiList = new ArrayList<WifiConfiguration>();
    }

    @Override
    public void core_run() throws Exception {
        readWiFiConfigurationIni();

        if (wm.isWifiEnabled()) {
            addLog("WIFI已打开");
        } else {
            addLog("首次打开WIFI" + (wm.setWifiEnabled(true) ? "成功" : "失败"));
            Thread.sleep(5000);
            if (!wm.isWifiEnabled()) {
                addLog("二次打开WIFI" + (wm.setWifiEnabled(true) ? "成功" : "失败"));
                Thread.sleep(5000);
                if (!wm.isWifiEnabled()) {
                    addLog("三次打开WIFI" + (wm.setWifiEnabled(true) ? "成功" : "失败"));
                    Thread.sleep(5000);
                }
            }
        }
        if (wm.isWifiEnabled()) {
            Thread.sleep(20000);
            needsQuit = isConnected();

            if (needsQuit) {
                addLog("已连接历史保存的WIFI");
            } else {
                scanWifiList();
                if (mWifiList.isEmpty()) {
                    if (TextUtils.isEmpty(configName)) {
                        addLog("请在电脑客户端配置WIFI");
                        needsQuit = true;
                    } else {
                        addLog("请检查配置的WIFI[" + configName + "]存在不");
                        scanWifiList();
                        if (mWifiList.isEmpty()) {
                            needsQuit = true;
                        }
                    }
                } else {
                    if (TextUtils.isEmpty(configName)) {
                        addLog("开始连接默认WIFI[SamgsungShop*]");
                    } else {
                        addLog("开始连接配置WIFI[" + configName + "]");
                    }
                }

                while (!needsQuit) {
                    if (index < length) {
                        if (index == 0) {
                            addLog("执行第" + (count + 1) + "轮WIFI连接操作");
                        }
                        WifiConfiguration wc = mWifiList.get(index);
                        addAndEnableNetwork(wc);
                        index++;
                        Thread.sleep(30000);
                        needsQuit = isConnected();
                        addLog(index + "/" + length + ":" + wc.SSID + (needsQuit ? ", 已连接" : ", 未连接"));
                    } else {
                        count++;
                        index = 0;
                        if (count > 2) {
                            count = 0;
                            needsQuit = true;
                        } else {
                            scanWifiList();
                            if (mWifiList.isEmpty()) {
                                needsQuit = true;
                            }
                        }
                    }
                }
                addLog("WIFI连接操作已结束");
            }
        }
    }

    private void addLog(String log) {
        Intent intent = new Intent(ZMsg2.ZMSG2_BROADCAST_ACTION);
        intent.putExtra("cmd", ZMsg2.ZMSG2_CMD_ADD_LOG);
        intent.putExtra("time", System.currentTimeMillis());
        intent.putExtra("log", log);
        lbm.sendBroadcast(intent);
        Log.e(TAG, log);
    }

    public void scanWifiList() throws InterruptedException {
        if (wm.startScan()) {
            Log.e(TAG, "startScan() true");
            Thread.sleep(3000);
            List<ScanResult> tempList = wm.getScanResults();
            if (tempList.isEmpty()) {
                if (!TextUtils.isEmpty(configName) && !TextUtils.isEmpty(configPwd)) {
                    mWifiList.add(0, createWifiConfiguration(configName, configPwd, 3));
                }
            } else {
                for (ScanResult result : tempList) {
                    if (result.SSID.equals(configName) && !TextUtils.isEmpty(configPwd)) {
                        mWifiList.add(0, createWifiConfiguration(configName, configPwd, 3));
                    } else if (result.SSID.startsWith("SamgsungShop")) {
                        mWifiList.add(createWifiConfiguration(result.SSID, "741258963", 3));
                    }
                }
            }
        } else {
            Log.e(TAG, "startScan() false");
            scanWifiList();
        }
        length = mWifiList.size();
        Log.e(TAG, "scanWifiList() size=" + length);
    }

    public boolean isConnected() {
        boolean isConnected = false;
        try {
            URL url = new URL("http://www.baidu.com");
            HttpURLConnection conn = (HttpURLConnection) url.openConnection();
            conn.setRequestMethod("GET");
            conn.setConnectTimeout(5000);
            if (conn.getResponseCode() == 200) {
                isConnected = true;
            }
        } catch (Exception e) {
            NetworkInfo ni = cm.getActiveNetworkInfo();
            if (ni != null) {
                if (ni.isAvailable() && ni.isConnected()) {
                    isConnected = true;
                }
            }
        }
        return isConnected;
    }

    public WifiConfiguration isExisted(String ssid) {
        List<WifiConfiguration> list = wm.getConfiguredNetworks();
        if (list != null) {
            for (WifiConfiguration wc : list) {
                if (wc.SSID.equals("\"" + ssid + "\"")) {
                    return wc;
                }
            }
        }
        return null;
    }

    public boolean addAndEnableNetwork(WifiConfiguration config) {
        int netId = wm.addNetwork(config);
        return wm.enableNetwork(netId, true);
    }

    public String[] readWiFiConfigurationIni() {
        String[] content = null;
        File file = new File("/data/local/tmp/WiFiConfiguration.ini");
        if (file.isFile() && file.exists()) {
            try {
                InputStreamReader read = new InputStreamReader(new FileInputStream(file), "UTF-8");
                BufferedReader br = new BufferedReader(read);
                String line = null;
                while ((line = br.readLine()) != null) {
                    if (line.contains("username=")) {
                        configName = line.replace("username=", "");
                    } else if (line.contains("password=")) {
                        configPwd = line.replace("password=", "");
                    }
                }
                content = new String[] { configName, configPwd };
                addLog("ConfigWifi:" + configName + "/" + configPwd);
                read.close();
                br.close();
            } catch (Exception e) {
                e.printStackTrace();
            }
        }
        return content;
    }

    public WifiConfiguration createWifiConfiguration(String ssid, String password, int type) {
        Log.e(TAG, "ssid:" + ssid + ", password:" + password + ", type:" + type);
        WifiConfiguration config = new WifiConfiguration();
        config.allowedAuthAlgorithms.clear();
        config.allowedGroupCiphers.clear();
        config.allowedKeyManagement.clear();
        config.allowedPairwiseCiphers.clear();
        config.allowedProtocols.clear();
        config.SSID = "\"" + ssid + "\"";

        WifiConfiguration tempConfig = isExisted(ssid);

        if (tempConfig != null) {
            // wm.removeNetwork(tempConfig.networkId);
            return tempConfig;
        }

        if (type == 1) { // WIFICIPHER_NOPASS
            config.wepKeys[0] = "";
            config.allowedKeyManagement.set(WifiConfiguration.KeyMgmt.NONE);
            config.wepTxKeyIndex = 0;
        }
        if (type == 2) { // WIFICIPHER_WEP
            config.hiddenSSID = true;
            config.wepKeys[0] = "\"" + password + "\"";
            config.allowedAuthAlgorithms.set(WifiConfiguration.AuthAlgorithm.SHARED);
            config.allowedGroupCiphers.set(WifiConfiguration.GroupCipher.CCMP);
            config.allowedGroupCiphers.set(WifiConfiguration.GroupCipher.TKIP);
            config.allowedGroupCiphers.set(WifiConfiguration.GroupCipher.WEP40);
            config.allowedGroupCiphers.set(WifiConfiguration.GroupCipher.WEP104);
            config.allowedKeyManagement.set(WifiConfiguration.KeyMgmt.NONE);
            config.wepTxKeyIndex = 0;
        }
        if (type == 3) { // WIFICIPHER_WPA
            config.preSharedKey = "\"" + password + "\"";
            config.hiddenSSID = true;
            config.allowedAuthAlgorithms.set(WifiConfiguration.AuthAlgorithm.OPEN);
            config.allowedGroupCiphers.set(WifiConfiguration.GroupCipher.TKIP);
            config.allowedKeyManagement.set(WifiConfiguration.KeyMgmt.WPA_PSK);
            config.allowedPairwiseCiphers.set(WifiConfiguration.PairwiseCipher.TKIP);
            // config.allowedProtocols.set(WifiConfiguration.Protocol.WPA);
            config.allowedGroupCiphers.set(WifiConfiguration.GroupCipher.CCMP);
            config.allowedPairwiseCiphers.set(WifiConfiguration.PairwiseCipher.CCMP);
            config.status = WifiConfiguration.Status.ENABLED;
        }
        return config;
    }

}
