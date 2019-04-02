package com.dx.utils;

import java.util.ArrayList;
import java.util.List;

import android.content.Context;
import android.content.SharedPreferences;
import android.text.TextUtils;
import android.util.DisplayMetrics;
import android.widget.LinearLayout;

/**
 * SharedPreferences工具类
 * 
 * @author guozg
 * 
 */
public final class SpUtil {
    static final String TAG = SpUtil.class.getSimpleName();

    private static Context mContext;
    public static SpUtil mInstance;
    private static SharedPreferences mSp;
    public static final String SP_DIALOG_WIDTH = "sp_dialog_width";// 公共Dialog宽度
    public static final String SP_SCREEN_WIDTH = "sp_screen_width";// 屏幕宽度
    public static final String SP_SCREEN_HEIGHT = "sp_screen_height";// 屏幕高度
    public static final String SP_LOG = "sp_log";
    public static final String SP_IS_YUNOS = "sp_is_yunos";// 手机系统是否是YunOS
    public static final String SP_ALL_START_APPS = "sp_all_start_apps";// 所有需要启动的应用包名
    public static final String SP_INSTALLED_APPS = "sp_installed_apps";// 已安装推广应用的包名
    public static final String SP_NEEDS_START_APPS = "sp_needs_start_apps";// 真正需要启动的应用包名
    public static final String SP_LAST_START_FINISHED_TIME = "sp_last_start_finished_time";// 上次启动完成时间
    public static final String SP_LAST_CHECK_TIME = "sp_last_check_time";// 最后一次检测激活操作的时间
    public static final String SP_ASYNC_UPLOAD_URL = "sp_async_upload_url";// 异步安装上报URL
    public static final String SP_INSTALL_RESULT = "sp_install_result";// 安装结果
    public static final String SP_UPLOAD_RESULT = "sp_upload_result";// 安装上报结果
    // 版本低安装失败
    public static final String SP_ASYNC_UPLOAD_LOW_APKIDS = "sp_async_upload_low_apkids";
    public static final String SP_ASYNC_UPLOAD_LOW_NEWVERSIONS = "sp_async_upload_low_new_versions";
    public static final String SP_ASYNC_UPLOAD_LOW_CLIENTVERSION = "sp_async_upload_low_client_version";
    public static final String SP_ASYNC_UPLOAD_LOW_RESULT = "sp_upload_low_result";// 上报结果

    /** 启动服务的次数 **/
    public static final String SP_STARTED_COUNT = "sp_started_count";
    /** 最后一次启动时间 **/
    public static final String SP_LAST_START_TIME = "sp_last_start_time";
    /** 首次检测到插卡的时间 **/
    public static final String SP_FIRST_SIM_CARD_TIME = "sp_first_sim_card_time";
    /** 是否需要检测插卡时间 **/
    public static final String SP_NEEDS_CHECK_SIM_CARD_TIME = "sp_needs_check_sim_card_time";

    public static final String SP_INDEX = "sp_index";
    public static final String SP_I = "sp_i";

    private SpUtil() {
    }

    public static SpUtil getInstance(Context context) {
        if (mInstance == null) {
            mContext = context;
            mSp = mContext.getSharedPreferences(mContext.getPackageName(), Context.MODE_PRIVATE);
            mSp.edit().commit();
            return new SpUtil();
        }
        return mInstance;
    }

    public String getString(String key, String defValue) {
        return mSp.getString(key, defValue);
    }

    public int getInt(String key, int defValue) {
        return mSp.getInt(key, defValue);
    }

    public long getLong(String key, int defValue) {
        return mSp.getLong(key, defValue);
    }

    public boolean getBoolean(String key, boolean defValue) {
        return mSp.getBoolean(key, defValue);
    }

    public void putString(String key, String value) {
        mSp.edit().putString(key, value).commit();
    }

    public void putInt(String key, int value) {
        mSp.edit().putInt(key, value).commit();
    }

    public void putLong(String key, long value) {
        mSp.edit().putLong(key, value).commit();
    }

    public void putBoolean(String key, boolean value) {
        mSp.edit().putBoolean(key, value).commit();
    }

    public void removeByKey(String key) {
        mSp.edit().remove(key).commit();
    }

    /**
     * 获取SP中保存的公共Dialog宽度
     * 
     * @author guozg
     * @param isWrapContent
     *            是否包装内容
     * @return isWrapContent=true，return
     *         LinearLayout.LayoutParams.WRAP_CONTENT；false，return 屏宽*0.8
     */
    public int getDialogWidth(boolean isWrapContent) {
        int dw = getInt(SP_DIALOG_WIDTH, 0);
        if (dw == 0) {
            if (isWrapContent) {
                dw = LinearLayout.LayoutParams.WRAP_CONTENT;
            } else {
                DisplayMetrics dm = mContext.getResources().getDisplayMetrics();// 获得屏幕参数：主要是分辨率，像素等。
                dw = (int) (dm.widthPixels * 0.9);
                putInt(SP_DIALOG_WIDTH, dw);
            }
        }
        return dw;
    }

    /**
     * 获取屏幕宽度
     * 
     * @return
     */
    public int getScreenWidth() {
        int sw = getInt(SP_SCREEN_WIDTH, 0);
        if (sw == 0) {
            DisplayMetrics dm = mContext.getResources().getDisplayMetrics();// 获得屏幕参数：主要是分辨率，像素等。
            sw = dm.widthPixels;
        }
        // Log.v(TAG, "getScreenWidth() " + sw);
        return sw;
    }

    /**
     * 获取屏幕高度
     * 
     * @return
     */
    public int getScreenHeight() {
        int sh = getInt(SP_SCREEN_HEIGHT, 0);
        if (sh == 0) {
            DisplayMetrics dm = mContext.getResources().getDisplayMetrics();// 获得屏幕参数：主要是分辨率，像素等。
            sh = dm.heightPixels;
        }
        Log.v(TAG, "getScreenHeight() " + sh);
        return sh;
    }

    public boolean isKeyExist(String key) {
        return mSp.contains(key);
    }

    /** 是否是YunOS **/
    public boolean isYunOs() {
        return getBoolean(SP_IS_YUNOS, false);
    }

    /** 距离装机结束时间大于10分钟 **/
    public boolean toTimeOut() {
        return System.currentTimeMillis() - getLong(SP_LAST_START_FINISHED_TIME, 0) > 10 * 60000;
    }

    /** 获取最后一次启动时间 **/
    public long getLastStartTime() {
        long t = getLong(SP_LAST_START_TIME, 0);
        return t;
    }

    /** 修改最后一次启动时间 **/
    public void putLastStartTime(long t) {
        putLong(SP_LAST_START_TIME, t);
    }

    public long getFirstSimCardTime() {
        return getLong(SP_FIRST_SIM_CARD_TIME, 0);
    }

    public void putFirstSimCardTime(long t) {
        putLong(SP_FIRST_SIM_CARD_TIME, t);
    }

    public int getStartedCount() {
        return getInt(SP_STARTED_COUNT, 0);
    }

    public void putStartedCount(int count) {
        putInt(SP_STARTED_COUNT, count);
    }

    public void putAllPkgs(String value) {
        putString(SP_ALL_START_APPS, value);
    }

    public String getAllPkgs() {
        return getString(SP_ALL_START_APPS, "");
    }

    /** 是否需要检测插卡时间，默认true **/
    public boolean needsCheckSimCardTime() {
        return getBoolean(SP_NEEDS_CHECK_SIM_CARD_TIME, true);
    }

    public void putPkg(String key, String pkg) {
        String pkgs = getPkgs(key);
        if (TextUtils.isEmpty(pkgs)) {
            pkgs = pkg;
        } else {
            if (!pkgs.contains(pkg)) {
                pkgs = pkg + "]" + pkgs;
            }
        }
        putString(key, pkgs);
    }

    public String getPkgs(String key) {
        return getString(key, "");
    }

    public void clearPkgList() {
        putString(SP_NEEDS_START_APPS, "");
        putString(SP_INSTALLED_APPS, "");
    }

    public List<String> getPkgList(String key) {
        String strPkgs = getPkgs(key);
        List<String> pkgList = new ArrayList<String>();
        if (!TextUtils.isEmpty(strPkgs)) {
            try {
                String[] pkgs = strPkgs.split("]");
                for (String pkg : pkgs) {
                    if ("net.dx.cloudgame".equals(pkg)) {
                        pkgList.add(0, pkg);
                    } else {
                        pkgList.add(pkg);
                    }
                }
                // return Arrays.asList(pkgs);
            } catch (Exception e) {
                e.printStackTrace();
            }
        }
        return pkgList;
    }

}
