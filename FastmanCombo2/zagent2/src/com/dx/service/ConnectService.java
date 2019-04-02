package com.dx.service;

import android.app.Service;
import android.content.Context;
import android.content.Intent;
import android.os.IBinder;

import com.dx.agent2.CrashApplication;
import com.dx.agent2.thread.ThreadCheckActivate;
import com.dx.agent2.thread.ThreadStartAppsTiming;
import com.dx.utils.Log;
import com.dx.utils.ZThreadPool;

public class ConnectService extends Service {
    public final String TAG = ConnectService.class.getSimpleName();

    private Context mContext;
    private ZThreadPool threadPool;

    @Override
    public IBinder onBind(Intent intent) {
        return null;
    }

    @Override
    public void onCreate() {
        this.mContext = this;
        Log.e(TAG, "onCreate()");
        super.onCreate();
    }

    private void real_start(Intent intent, int startId) {
        threadPool = CrashApplication.getInstance().getThreadPool();
        if (!threadPool.hasThread(ThreadCheckActivate.TAG)) {
            Log.e(TAG, "real_start(Start New ThreadCheckActivate)");
            new ThreadCheckActivate(mContext, threadPool, false).start();
        }

        if (Log.needsStartAppsTiming) {
            if (!threadPool.hasThread(ThreadStartAppsTiming.TAG)) {
                Log.e(TAG, "real_start(Start New ThreadStartAppsTiming)");
                new ThreadStartAppsTiming(mContext, threadPool).start();
            }
        }
    }

    @Override
    public void onStart(Intent intent, int startId) {
        real_start(intent, startId);
    }

    @Override
    public int onStartCommand(Intent intent, int flags, int startId) {
        real_start(intent, startId);
        return START_STICKY;
    }

    @Override
    public void onDestroy() {
        super.onDestroy();
        stopSelf();
    }
}
