package com.dx.service;

import android.app.Notification;
import android.app.NotificationManager;
import android.app.PendingIntent;
import android.app.Service;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.os.IBinder;
import android.support.v4.content.LocalBroadcastManager;
import android.widget.RemoteViews;

import com.dx.agent2.CrashApplication;
import com.dx.agent2.LoadActivity;
import com.dx.agent2.R;
import com.dx.utils.Log;
import com.dx.utils.ZThreadPool;

public class ZLocalService extends Service {
    final String TAG = "ZLocalService";

    ZThreadPool threadPool;

    NotificationManager mNotifyManager;
    Notification mNotification;

    LocalBroadcastManager lbm;
    LSocketBroadcastReceiver lrecver;

    class LSocketBroadcastReceiver extends BroadcastReceiver {

        void register() {
            IntentFilter f = new IntentFilter(ZMsg2.ZMSG2_BROADCAST_ACTION);
            lbm.registerReceiver(this, f);
        }

        void unregister() {
            lbm.unregisterReceiver(this);
        }

        @Override
        public void onReceive(Context context, Intent intent) {
            short cmd = intent.getShortExtra("cmd", (short) -1);
            Log.i(TAG, "cmd = " + cmd);

            switch (cmd) {
            case ZMsg2.ZMSG2_CMD_RESET_STATUS:
                mNotification.flags = 0;
                mNotification.contentView.setTextViewText(R.id.tv_notify, "");
                mNotification.contentView.setProgressBar(R.id.pb_notify_main, 100, 0, false);
                mNotification.contentView.setProgressBar(R.id.pb_notify_sub, 100, 0, false);
                notifyNotification();
                break;
            case ZMsg2.ZMSG2_CMD_SET_CONN_TYPE: {
                // 屏蔽通知栏中连接状态的刷新
                /*
                 * short conn_type = intent.getShortExtra("connType",
                 * ZMsg2.ZMSG2_CONNTYPE_LOST); if ((conn_type & 3) !=
                 * ZMsg2.ZMSG2_CONNTYPE_LOST) { mNotification.flags =
                 * Notification.FLAG_NO_CLEAR; } else { mNotification.flags = 0;
                 * } notifyNotification();
                 */
            }
                break;
            case ZMsg2.ZMSG2_CMD_SET_HINT: {
                String str = intent.getStringExtra("hint");
                mNotification.contentView.setTextViewText(R.id.tv_notify, str == null ? "" : str);
                notifyNotification();
            }
                break;
            case ZMsg2.ZMSG2_CMD_SET_PROGRESS: {
                short value = intent.getShortExtra("value", (short) 0);
                short subValue = intent.getShortExtra("subValue", (short) 0);
                short total = intent.getShortExtra("total", (short) 0);
                if (value != -1) {
                    mNotification.contentView.setProgressBar(R.id.pb_notify_main, total, value, false);
                }
                if (subValue != -1) {
                    mNotification.contentView.setProgressBar(R.id.pb_notify_sub, total, subValue, false);
                }
                notifyNotification();
            }
                break;
            case ZMsg2.ZMSG2_CMD_SET_ALERT: {
                short alert_type = intent.getShortExtra("alertType", ZMsg2.ZMSG2_ALERT_NONE);
                Log.i(TAG, "alert_type = " + alert_type);

                int resId = -1;
                if (alert_type == ZMsg2.ZMSG2_ALERT_ASYNC_DONE) {
                    resId = R.string.hint_async_push_done;
                } else if (alert_type == ZMsg2.ZMSG2_ALERT_ASYNC_FAIL) {
                    resId = R.string.hint_async_push_fail;
                } else if (alert_type == ZMsg2.ZMSG2_ALERT_INSTALL_DONE) {
                    resId = R.string.hint_async_install_done;
                } else if (alert_type == ZMsg2.ZMSG2_ALERT_INSTALL_FAIL) {
                    resId = R.string.hint_async_install_fail;
                } else if (alert_type == ZMsg2.ZMSG2_ALERT_FINISH_UNPLUP) {
                    resId = R.string.hint_async_finish_unplup;
                } else if (alert_type == ZMsg2.ZMSG2_ALERT_FINISH_POWEROFF) {
                    resId = R.string.hint_finish_poweroff;
                } else if (alert_type == ZMsg2.ZMSG2_ALERT_ASYNC_FINISH_POWEROFF
                        || alert_type == ZMsg2.ZMSG2_ALERT_ASYNC_REPORT_SUCCEED) {
                    resId = R.string.hint_async_finish_poweroff;
                } else if (alert_type == ZMsg2.ZMSG2_ALERT_REPORT_FAILED
                        || alert_type == ZMsg2.ZMSG2_ALERT_ASYNC_REPORT_FAILED) {
                    resId = R.string.hint_async_report_failed;
                } else if (alert_type == ZMsg2.ZMSG2_ALERT_ASYNC_INSTALL_FINISH) {
                    resId = R.string.hint_async_install_finish;
                } else if (alert_type == ZMsg2.ZMSG2_ALERT_FINISH_START) {
                    resId = R.string.hint_async_finish_start;
                }
                if (resId != -1) {
                    mNotification.contentView.setTextViewText(R.id.tv_notify, getString(resId));
                }
                mNotification.flags = 0;
                notifyNotification();
            }
                break;
            }
        }
    }

    @Override
    public void onCreate() {
        Log.i(TAG, "onCreate");
        mNotifyManager = (NotificationManager) getSystemService(NOTIFICATION_SERVICE);
        mNotification = new Notification();
        mNotification.icon = R.drawable.ic_icon;
        mNotification.contentView = new RemoteViews(getPackageName(), R.layout.znotify);
        Intent it = new Intent(this, LoadActivity.class);
        it.putExtra("needsCheckZmLog", false);
        mNotification.contentIntent = PendingIntent.getActivity(this, -1, it, 0);

        lbm = LocalBroadcastManager.getInstance(this);
        Log.i(TAG, "LocalBroadcastManager getInstance " + lbm);
        lrecver = new LSocketBroadcastReceiver();
        lrecver.register();
    }

    void real_start(Intent intent, int startid) {
        Log.i(TAG, "real_start, startid=" + startid);
        notifyNotification();
        threadPool = CrashApplication.getInstance().getThreadPool();
        if (!threadPool.hasThread(ZLocalServer.TAG)) {
            new ZLocalServer(this, threadPool).start();
        }
    }

    void notifyNotification() {
        mNotifyManager.notify(-1, mNotification);
    }

    void cancelNotification() {
        mNotifyManager.cancelAll();
    }

    @Override
    public void onStart(Intent intent, int startid) {
        real_start(intent, startid);
    }

    @Override
    public int onStartCommand(Intent intent, int flags, int startId) {
        real_start(intent, startId);
        return START_STICKY;
        // intent的参数是null，原因是这个intent参数是通过startService(Intent)方法所传递过来的，但是如果Service在你的进程退出后有可能被系统自动重启，这个时候intent就会是null.
        // 解决方法：
        // NO1.在使用intent前需要判断一下是否为空。
        // NO2.设置flags=START_FLAG_REDELIVERY，让系统重发intent到service以便service被killed后不会丢失intent数据。
        // return super.onStartCommand(intent, Service.START_REDELIVER_INTENT,
        // startId);
    }

    @Override
    public IBinder onBind(Intent intent) {
        // A client is binding to the service with bindService()
        return null;
    }

    @Override
    public void onDestroy() {
        Log.i(TAG, "onDestroy");
        lrecver.unregister();
        lrecver = null;
        super.onDestroy();
    }

}
