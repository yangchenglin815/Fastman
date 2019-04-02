package com.dx.utils;

import java.io.BufferedReader;
import java.io.File;
import java.io.FileInputStream;
import java.io.FileReader;
import java.io.IOException;
import java.security.MessageDigest;
import java.util.Arrays;

import android.content.Context;
import android.content.Intent;
import android.content.pm.ApplicationInfo;
import android.content.pm.PackageInfo;
import android.content.pm.PackageManager;
import android.content.pm.PackageManager.NameNotFoundException;
import android.net.ConnectivityManager;
import android.net.NetworkInfo;
import android.net.TrafficStats;
import android.net.Uri;
import android.os.Build;
import android.os.Environment;

public class ZAndroidUtil {
    private static final String TAG = "ZAndroidUtil";

    protected static MessageDigest messagedigest = null;
    static {
        try {
            messagedigest = MessageDigest.getInstance("MD5");
        } catch (Exception e) {
            e.printStackTrace();
        }
    }

    protected static char hexDigits[] = { '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'a', 'b', 'c', 'd', 'e',
            'f' };

    private static String toHexString(byte[] bytes) {
        StringBuffer stringbuffer = new StringBuffer(2 * bytes.length);
        for (int l = 0; l < bytes.length; l++) {
            char c0 = hexDigits[(bytes[l] & 0xf0) >> 4];
            char c1 = hexDigits[bytes[l] & 0xf];
            stringbuffer.append(c0);
            stringbuffer.append(c1);
        }
        return stringbuffer.toString();
    }

    public static String getMd5(byte[] bytes) {
        try {
            messagedigest.update(bytes);
            return toHexString(messagedigest.digest());
        } catch (Exception e) {
            e.printStackTrace();
        }
        return "";
    }

    public static String getStringMd5(String str) {
        if (str == null || str.length() == 0) {
            return "";
        }
        return getMd5(str.getBytes());
    }

    public static String getFileMd5(String path) {
        if (path == null || path.length() == 0) {
            return "";
        }

        try {
            byte[] buf = new byte[4096];
            int n = 0;

            FileInputStream fis = new FileInputStream(path);
            while ((n = fis.read(buf, 0, 4096)) > 0) {
                messagedigest.update(buf, 0, n);
            }
            fis.close();

            return toHexString(messagedigest.digest());
        } catch (Exception e) {
            e.printStackTrace();
        }
        return "";
    }

    public static String getApkPath(String pkg, Context context) {
        PackageManager pm = context.getPackageManager();
        try {
            PackageInfo info = pm.getPackageInfo(pkg, 0);
            return info.applicationInfo.sourceDir;
        } catch (Exception e) {
            // e.printStackTrace();
        }
        return "";
    }

    public static long oldGetAppFlow(String pkg, Context context) {
        if (Build.VERSION.SDK_INT < 8) {
            Log.i(TAG, "getAppFlow SDK_INT < 8");
            return -1;
        }
        if (TrafficStats.getTotalRxBytes() == TrafficStats.UNSUPPORTED) {
            Log.i(TAG, "getAppFlow UNSUPPORTED");
            return -1;
        }

        try {
            PackageManager pm = context.getPackageManager();
            ApplicationInfo info = pm.getApplicationInfo(pkg, 0);
            int uid = info.uid;
            long flow = TrafficStats.getUidRxBytes(uid) + TrafficStats.getUidTxBytes(uid);
            Log.i(TAG, "getAppFlow:" + pkg + ", flow=" + flow);
            return flow;
        } catch (Exception e) {
            // e.printStackTrace();
        }
        return -1;
    }

    /** 根据包名获取该应用的流量值 **/
    public static long getAppFlow(String packageName, Context context) {
        long flow = 0;
        try {
            PackageManager pm = context.getPackageManager();
            PackageInfo pkgInfo = pm.getPackageInfo(packageName, 0);
            ApplicationInfo appInfo = pkgInfo.applicationInfo;
            if (appInfo != null) {
                flow = getAppFlow(appInfo);
            }
        } catch (NameNotFoundException e) {
            Log.v(TAG, packageName + " 已卸载！");
            flow = -1;
        }
        return flow;
    }

    private static long getAppFlow(ApplicationInfo appInfo) {
        if (Build.VERSION.SDK_INT < 8 || TrafficStats.getTotalRxBytes() == TrafficStats.UNSUPPORTED)
            return 0;
        try {
            int uid = appInfo.uid;
            // 某一个进程的总接收量+总发送量
            long flow4Rx = TrafficStats.getUidRxBytes(uid);
            long flow4Tx = TrafficStats.getUidTxBytes(uid);
            long flow = flow4Rx == TrafficStats.UNSUPPORTED ? 0 : flow4Rx + flow4Tx == TrafficStats.UNSUPPORTED ? 0
                    : flow4Tx;
            if (flow == 0) {
                flow = getTotalBytesManual(uid);
            }
            Log.v(TAG, "getAppFlow() " + appInfo.packageName + " , flow = " + flow);
            return flow;
        } catch (Exception e) {
            e.printStackTrace();
        }
        return 0;
    }

    private static Long getTotalBytesManual(int localUid) {
        File dir = new File("/proc/uid_stat/");
        String[] children = dir.list();
        if (!Arrays.asList(children).contains(String.valueOf(localUid))) {
            return 0L;
        }
        File uidFileDir = new File("/proc/uid_stat/" + String.valueOf(localUid));
        File uidActualFileReceived = new File(uidFileDir, "tcp_rcv");
        File uidActualFileSent = new File(uidFileDir, "tcp_snd");

        String textReceived = "0";
        String textSent = "0";
        try {
            BufferedReader brReceived = new BufferedReader(new FileReader(uidActualFileReceived));
            BufferedReader brSent = new BufferedReader(new FileReader(uidActualFileSent));
            String receivedLine;
            String sentLine;
            if ((receivedLine = brReceived.readLine()) != null) {
                textReceived = receivedLine;
            }
            if ((sentLine = brSent.readLine()) != null) {
                textSent = sentLine;
            }
            brReceived.close();
            brSent.close();
        } catch (IOException e) {
        }
        return Long.valueOf(textReceived).longValue() + Long.valueOf(textSent).longValue();
    }

    public static NetworkInfo getNetworkInfo(Context context) {
        ConnectivityManager cm = (ConnectivityManager) context.getSystemService(Context.CONNECTIVITY_SERVICE);
        NetworkInfo info = cm.getActiveNetworkInfo();
        if (info != null) {
            Log.i(TAG, "getActiveNetworkInfo type = " + info.getTypeName() + ", connected = " + info.isConnected());
        } else {
            Log.i(TAG, "getActiveNetworkInfo null");
        }
        return info;
    }

    public static boolean hasSdCard() {
        String state = Environment.getExternalStorageState();
        Log.i(TAG, "getExternalStorageState: " + state);
        if (state.equals(Environment.MEDIA_MOUNTED)) {
            Log.i(TAG, "[+] SD card exists");
            return true;
        }
        return false;
    }

    public static void startAppDetail(Context context, String packageName) {
        final String SCHEME = "package";
        /** 调用系统InstalledAppDetails界面所需的Extra名称(用于Android 2.1及之前版本) */
        final String APP_PKG_NAME_21 = "com.android.settings.ApplicationPkgName";
        /** 调用系统InstalledAppDetails界面所需的Extra名称(用于Android 2.2) */
        final String APP_PKG_NAME_22 = "pkg";
        /** InstalledAppDetails所在包名 */
        final String APP_DETAILS_PACKAGE_NAME = "com.android.settings";
        /** InstalledAppDetails类名 */
        final String APP_DETAILS_CLASS_NAME = "com.android.settings.InstalledAppDetails";

        Intent intent = new Intent();
        final int apiLevel = Build.VERSION.SDK_INT;
        if (apiLevel >= 9) { // 2.3（ApiLevel 9）以上，使用SDK提供的接口
            intent.setAction("android.settings.APPLICATION_DETAILS_SETTINGS");
            Uri uri = Uri.fromParts(SCHEME, packageName, null);
            intent.setData(uri);
        } else { // 2.3以下，使用非公开的接口（查看InstalledAppDetails源码）
            // 2.2和2.1中，InstalledAppDetails使用的APP_PKG_NAME不同。
            final String appPkgName = (apiLevel == 8 ? APP_PKG_NAME_22 : APP_PKG_NAME_21);
            intent.setAction(Intent.ACTION_VIEW);
            intent.setClassName(APP_DETAILS_PACKAGE_NAME, APP_DETAILS_CLASS_NAME);
            intent.putExtra(appPkgName, packageName);
        }
        context.startActivity(intent);
    }

    public static void startAppUninstall(Context context, String pkg) {
        String uriString = "package:" + pkg;
        Uri uninstallUrl = Uri.parse(uriString);
        Intent intent = new Intent(Intent.ACTION_DELETE, uninstallUrl);
        context.startActivity(intent);
    }

    public static void shareFile(Context context, String path, String type) {
        try {
            Intent intent = new Intent();
            intent.setAction(Intent.ACTION_SEND);
            intent.setType(type);
            intent.putExtra(Intent.EXTRA_STREAM, Uri.fromFile(new File(path)));
            context.startActivity(intent);
        } catch (Exception e) {
            e.printStackTrace();
        }
    }
}
