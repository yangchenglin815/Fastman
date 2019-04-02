package com.dx.agent2;

import android.app.Application;
import android.content.pm.PackageInfo;
import android.content.pm.PackageManager;
import android.content.pm.PackageManager.NameNotFoundException;
import android.text.TextUtils;

import com.dx.utils.Log;
import com.dx.utils.ZThreadPool;

public class CrashApplication extends Application {
    final String TAG = "CrashApplication";

    private static CrashApplication mInstance;
    private ZThreadPool mThreadPool;
    private String mVersionName = "";

    public static CrashApplication getInstance() {
        return mInstance;
    }

    public ZThreadPool getThreadPool() {
        if (mThreadPool == null) {
            mThreadPool = new ZThreadPool();
        }
        return mThreadPool;
    }

    @Override
    public void onCreate() {
        super.onCreate();
        mInstance = this;
        mThreadPool = new ZThreadPool();
        if (Log.needsLogCrash) {
            CrashHandler handler = CrashHandler.getInstance();
            handler.init(this);
        }
    }

    public String getVersionName() {
        if (TextUtils.isEmpty(mVersionName)) {
            try {
                PackageManager pm = this.getPackageManager();
                PackageInfo pi = pm.getPackageInfo(this.getPackageName(), PackageManager.GET_ACTIVITIES);
                if (pi != null) {
                    mVersionName = "V" + pi.versionName;
                }
                Log.e(TAG, mVersionName);
            } catch (NameNotFoundException e) {
                e.printStackTrace();
            }
        }
        return mVersionName;
    }

}
