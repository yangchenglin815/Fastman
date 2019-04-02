package com.dx.agent2;

import java.util.Calendar;
import java.util.Collections;
import java.util.List;
import java.util.Locale;
import java.util.Map;
import java.util.Map.Entry;
import java.util.Set;
import java.util.Timer;
import java.util.TimerTask;

import android.annotation.SuppressLint;
import android.app.Activity;
import android.app.AlertDialog;
import android.app.NotificationManager;
import android.app.admin.DevicePolicyManager;
import android.content.BroadcastReceiver;
import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.media.AudioManager;
import android.media.MediaPlayer;
import android.media.RingtoneManager;
import android.net.Uri;
import android.os.AsyncTask;
import android.os.Build;
import android.os.Bundle;
import android.os.Handler;
import android.os.Message;
import android.os.Vibrator;
import android.support.v4.content.LocalBroadcastManager;
import android.text.TextUtils;
import android.view.Menu;
import android.view.MenuItem;
import android.view.View;
import android.view.View.OnClickListener;
import android.view.WindowManager;
import android.view.WindowManager.LayoutParams;
import android.widget.LinearLayout;
import android.widget.ProgressBar;
import android.widget.ScrollView;
import android.widget.TextView;

import com.dx.agent2.db.LogData;
import com.dx.agent2.db.LogDb;
import com.dx.agent2.thread.ThreadCheckActivate;
import com.dx.agent2.thread.ThreadCheckUrl;
import com.dx.agent2.thread.ThreadInstallReport;
import com.dx.agent2.thread.ThreadTopActivity;
import com.dx.service.ZLocalService;
import com.dx.service.ZMsg2;
import com.dx.utils.IPhoneSubInfoUtil;
import com.dx.utils.Log;
import com.dx.utils.SpUtil;
import com.dx.utils.ZThreadPool;

public class MainActivity extends Activity {
    final String TAG = "MainActivity";

    TextView tv_version;
    TextView tv_index;
    TextView tv_status;
    TextView tv_hint;
    TextView tv_log;
    ProgressBar pb_main;
    ScrollView sv_log;
    LinearLayout layout_header;

    LocalBroadcastManager lbm;
    LSocketBroadcastReceiver lrecver;

    SpUtil spUtil;
    LogDb logDb;
    String log;
    ZThreadPool threadPool;

    DevicePolicyManager dpm;
    ComponentName adminComponent;

    String fmtTime(long tm) {
        Calendar c = Calendar.getInstance();
        c.setTimeInMillis(tm);
        return String.format(Locale.getDefault(), "%02d:%02d:%02d: ", c.get(Calendar.HOUR_OF_DAY),
                c.get(Calendar.MINUTE), c.get(Calendar.SECOND));
    }

    int lostCount = 0;

    void refreshConnType(short conn_type) {
        short type = (short) (conn_type & 3);
        short index = (short) (conn_type >> 2);
        lastIndex = index;
        Log.i(TAG, "refreshConnType index=" + index + ", type=" + type);

        switch (type) {
        case ZMsg2.ZMSG2_CONNTYPE_USB:
            lostCount = 0;
            lastConnectedTime = System.currentTimeMillis();
            tv_status.setText(R.string.conn_usb);
            layout_header.setBackgroundResource(R.drawable.bg_green);
            break;
        case ZMsg2.ZMSG2_CONNTYPE_WIFI:
            lostCount = 0;
            lastConnectedTime = System.currentTimeMillis();
            tv_status.setText(R.string.conn_wifi);
            layout_header.setBackgroundResource(R.drawable.bg_green);
            break;
        default:
            lostCount++;
            tv_status.setText(R.string.conn_lost);
            layout_header.setBackgroundResource(R.drawable.bg_red);
            break;
        }

        tv_index.setText(index > 0 ? Short.toString(index) : "?");
        if (lostCount >= 10) {
            lostConnectTimer.cancel();
            Log.e(TAG, "lostCount>=10 cancel timer");
        }
    }

    MediaPlayer audioplayer;
    Vibrator vibrator;

    void refreshAlertType(short alert_type) {
        boolean needsSound = false, needsVibrate = false;
        int hintResId = -1;
        int bgResId = R.drawable.bg_grey;

        if (alert_type == ZMsg2.ZMSG2_ALERT_ASYNC_DONE) {// 异步：拷贝已完成，不变屏
            hintResId = R.string.hint_async_push_done;
        } else if (alert_type == ZMsg2.ZMSG2_ALERT_ASYNC_FAIL) {// 异步：拷贝失败，不变屏
            hintResId = R.string.hint_async_push_fail;
        } else if (alert_type == ZMsg2.ZMSG2_ALERT_INSTALL_DONE) {// 异步/同步：安装已完成，不变屏
            hintResId = R.string.hint_async_install_done;
        } else if (alert_type == ZMsg2.ZMSG2_ALERT_INSTALL_FAIL) {// 异步/同步：部分安装失败，不变屏
            hintResId = R.string.hint_async_install_fail;
        } else if (alert_type == ZMsg2.ZMSG2_ALERT_FINISH_UNPLUP) {// 异步：可以拔线，等待安装结束，变蓝屏
            hintResId = R.string.hint_async_finish_unplup;
            bgResId = R.drawable.bg_blue;
            needsSound = true;
            needsVibrate = true;
        } else if (alert_type == ZMsg2.ZMSG2_ALERT_FINISH_POWEROFF) {// 同步：可以拔线，等待自启，变蓝屏
            hintResId = R.string.hint_finish_poweroff;
            bgResId = R.drawable.bg_blue;
            needsSound = true;
            needsVibrate = true;
        } else if (alert_type == ZMsg2.ZMSG2_ALERT_ASYNC_FINISH_POWEROFF
                || alert_type == ZMsg2.ZMSG2_ALERT_ASYNC_REPORT_SUCCEED) {// 异步：推广应用，等待自启，不变屏
            hintResId = R.string.hint_async_finish_poweroff;
        } else if (alert_type == ZMsg2.ZMSG2_ALERT_REPORT_FAILED || alert_type == ZMsg2.ZMSG2_ALERT_ASYNC_REPORT_FAILED) {
            // 同步||异步：上报失败，变红屏
            hintResId = R.string.hint_async_report_failed;
            bgResId = R.drawable.bg_red;
            needsSound = true;
            needsVibrate = true;
        } else if (alert_type == ZMsg2.ZMSG2_ALERT_ASYNC_INSTALL_FINISH) {// 异步：安装结束，开始上报，不变屏
            hintResId = R.string.hint_async_install_finish;
        } else if (alert_type == ZMsg2.ZMSG2_ALERT_FINISH_START) {// 已完成，可以关机，变绿屏
            hintResId = R.string.hint_async_finish_start;
            bgResId = R.drawable.bg_green;
            needsSound = true;
            needsVibrate = true;

            mHandler.sendEmptyMessageDelayed(CODE_SHOW_DIALOG, 1000);
        }

        if (hintResId != -1) {
            tv_hint.setText(hintResId);
        }
        sv_log.setBackgroundResource(bgResId);

        Log.i(TAG, alert_type + ", needsSound = " + needsSound + ", needsVibrate = " + needsVibrate);

        mHandler.removeMessages(CODE_STOP_RING);
        mHandler.removeMessages(CODE_STOP_VIBRATE);
        try {
            if (audioplayer != null) {
                if (needsSound) {
                    startRing();
                } else {
                    stopRing();
                }
            }

            if (vibrator != null) {
                if (needsVibrate) {
                    startVibrate();
                } else {
                    stopVibrate();
                }
            }
        } catch (Throwable t) {
            t.printStackTrace();
        }
    }

    void startRing() throws Exception {
        if (!audioplayer.isPlaying()) {
            Uri uri = RingtoneManager.getDefaultUri(RingtoneManager.TYPE_RINGTONE);
            audioplayer.setDataSource(this, uri);
            audioplayer.setAudioStreamType(AudioManager.STREAM_RING);
            audioplayer.setLooping(true);
            audioplayer.prepare();
            audioplayer.start();
        }
        mHandler.sendEmptyMessageDelayed(CODE_STOP_RING, 10000);
    }

    void stopRing() {
        try {
            if (audioplayer.isPlaying()) {
                audioplayer.stop();
                audioplayer.reset();
            }
        } catch (Exception e) {
            e.printStackTrace();
        }
    }

    void startVibrate() {
        long[] pattern = new long[] { 200, 100, 300, 200 };
        vibrator.vibrate(pattern, 0);
        mHandler.sendEmptyMessageDelayed(CODE_STOP_VIBRATE, 10000);
    }

    void stopVibrate() {
        vibrator.cancel();
    }

    void resetStatus() {
        spUtil.putLong(SpUtil.SP_LAST_START_FINISHED_TIME, 0);
        spUtil.putString(SpUtil.SP_INSTALL_RESULT, "");
        spUtil.putString(SpUtil.SP_UPLOAD_RESULT, "");
        clearLog();

        LogData l = new LogData();
        l.time = System.currentTimeMillis();
        l.log = Build.BRAND + "_" + Build.MODEL;
        setLog(l);
        l = null;

        tv_hint.setText("准备就绪");
        pb_main.setMax(100);
        pb_main.setProgress(0);
        pb_main.setSecondaryProgress(0);
        refreshConnType(ZMsg2.ZMSG2_CONNTYPE_LOST);
        refreshAlertType(ZMsg2.ZMSG2_ALERT_NONE);
    }

    void setHint(String str) {
        if (str != null) {
            tv_hint.setText(str);
        } else {
            tv_hint.setText("");
        }
    }

    void setProgress(int value, int subValue, int total) {
        if (value != -1) {
            pb_main.setProgress(value);
        }
        if (subValue != -1) {
            pb_main.setSecondaryProgress(subValue);
        }
        pb_main.setMax(total);
    }

    void setLog(LogData l) {
        if (l != null) {
            log = fmtTime(l.time) + l.log + "\n" + log;
            tv_log.setText(log);
        }
    }

    void clearLog() {
        log = "";
        tv_log.setText(log);
        logDb.removeAll(LogDb.TABLENAME);
    }

    void showResultDialog() {
        AlertDialog.Builder dlg = new AlertDialog.Builder(this);
        dlg.setTitle("提示");
        String hint = "安装结果：\n" + spUtil.getString(SpUtil.SP_INSTALL_RESULT, "") + "\n上报结果：\n"
                + spUtil.getString(SpUtil.SP_UPLOAD_RESULT, "");
        dlg.setMessage(hint);
        dlg.create().show();
    }

    void showImeiDialog() {
        Map<String, String> imeiMap = IPhoneSubInfoUtil.getImeiMap(this);
        Set<Entry<String, String>> sets = imeiMap.entrySet();
        String ret = "";
        for (Entry<String, String> set : sets) {
            ret += "imei:";
            ret += set.getKey();
            ret += ", imsi:";
            ret += set.getValue();
            ret += "\n";
        }

        String smallImei = IPhoneSubInfoUtil.getSmallestImei(this);
        String smallImsi = imeiMap.remove(smallImei);
        ret += "SmallestImei=" + smallImei + ", SmallestImsi=" + smallImsi;

        AlertDialog.Builder dlg = new AlertDialog.Builder(this);
        dlg.setTitle(R.string.menu_show_imei);
        dlg.setMessage(ret);
        dlg.create().show();
    }

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
            switch (cmd) {
            case ZMsg2.ZMSG2_CMD_RESET_STATUS:
                mHandler.sendEmptyMessage(CODE_RESET_STATUS);
                break;
            case ZMsg2.ZMSG2_CMD_SET_CONN_TYPE:
                mHandler.obtainMessage(CODE_SET_CONN_TYPE, intent).sendToTarget();
                break;
            case ZMsg2.ZMSG2_CMD_ADD_LOG:
                mHandler.obtainMessage(CODE_ADD_LOG, intent).sendToTarget();
                break;
            case ZMsg2.ZMSG2_CMD_SET_HINT:
                mHandler.obtainMessage(CODE_SET_HINT, intent).sendToTarget();
                break;
            case ZMsg2.ZMSG2_CMD_SET_PROGRESS:
                mHandler.obtainMessage(CODE_SET_PROGRESS, intent).sendToTarget();
                break;
            case ZMsg2.ZMSG2_CMD_SET_ALERT:
                mHandler.obtainMessage(CODE_SET_ALERT, intent).sendToTarget();
                break;
            }
        }

    }

    final int CODE_STOP_RING = -1;
    final int CODE_STOP_VIBRATE = -2;
    final int CODE_ADD_LOG = -3;
    final int CODE_SHOW_DIALOG = -4;
    final int CODE_RESET_STATUS = -5;
    final int CODE_SET_HINT = -6;
    final int CODE_SET_PROGRESS = -7;
    final int CODE_SET_ALERT = -8;
    final int CODE_SET_CONN_TYPE = -9;
    @SuppressLint("HandlerLeak")
    Handler mHandler = new Handler() {
        public void handleMessage(Message msg) {
            Intent intent = null;
            if (msg.what == CODE_STOP_RING) {
                stopRing();
            } else if (msg.what == CODE_STOP_VIBRATE) {
                stopVibrate();
            } else if (msg.what == CODE_ADD_LOG) {
                intent = (Intent) msg.obj;
                long time = intent.getLongExtra("time", -1);
                String log = intent.getStringExtra("log");
                if (!TextUtils.isEmpty(log)) {
                    LogData l = new LogData();
                    l.time = time;
                    l.log = log;
                    setLog(l);

                    logDb.setData(LogDb.TABLENAME, l);
                    Log.i(TAG, "after insert new logdata");
                    l = null;
                }
            } else if (msg.what == CODE_SHOW_DIALOG) {
                showResultDialog();
            } else if (msg.what == CODE_RESET_STATUS) {
                resetStatus();
            } else if (msg.what == CODE_SET_HINT) {
                intent = (Intent) msg.obj;
                String hint = intent.getStringExtra("hint");
                setHint(hint);
            } else if (msg.what == CODE_SET_PROGRESS) {
                intent = (Intent) msg.obj;
                short value = intent.getShortExtra("value", (short) 0);
                short subValue = intent.getShortExtra("subValue", (short) 0);
                short total = intent.getShortExtra("total", (short) 0);
                setProgress(value, subValue, total);
            } else if (msg.what == CODE_SET_ALERT) {
                intent = (Intent) msg.obj;
                short alertType = intent.getShortExtra("alertType", ZMsg2.ZMSG2_ALERT_NONE);
                refreshAlertType(alertType);
            } else if (msg.what == CODE_SET_CONN_TYPE) {
                intent = (Intent) msg.obj;
                short conn_type = intent.getShortExtra("connType", ZMsg2.ZMSG2_CONNTYPE_LOST);
                refreshConnType(conn_type);
            }
            intent = null;
        }
    };

    class LogTask extends AsyncTask<Void, Void, String> {

        @Override
        protected String doInBackground(Void... params) {
            StringBuffer log = new StringBuffer();
            List<LogData> oldLogs = logDb.getAllData(LogDb.TABLENAME);
            Log.i(TAG, "load oldLogs count " + oldLogs.size());
            if (oldLogs.size() > 1000) {
                logDb.removeAll(LogDb.TABLENAME);
                return "";
            }
            Collections.reverse(oldLogs);// 倒序
            for (LogData l : oldLogs) {
                if (l != null) {
                    log.append(fmtTime(l.time)).append(l.log).append("\n");
                }
            }
            return log.toString();
        }

        @Override
        protected void onPostExecute(String result) {
            super.onPostExecute(result);
            log = result;
            tv_log.setText(log);
        }
    }

    short lastIndex = -1;
    long lastConnectedTime = -1;
    Timer lostConnectTimer;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        getWindow().addFlags(
                WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON | LayoutParams.FLAG_TURN_SCREEN_ON
                        | LayoutParams.FLAG_DISMISS_KEYGUARD | LayoutParams.FLAG_SHOW_WHEN_LOCKED);
        setContentView(R.layout.zactivity_main);

        tv_version = (TextView) findViewById(R.id.tv_version);
        tv_index = (TextView) findViewById(R.id.tv_index);
        tv_status = (TextView) findViewById(R.id.tv_status);
        tv_hint = (TextView) findViewById(R.id.tv_hint);
        tv_log = (TextView) findViewById(R.id.tv_log);
        pb_main = (ProgressBar) findViewById(R.id.pb_main);
        sv_log = (ScrollView) findViewById(R.id.sv_log);
        layout_header = (LinearLayout) findViewById(R.id.layout_header);

        audioplayer = new MediaPlayer();
        vibrator = (Vibrator) getSystemService(VIBRATOR_SERVICE);

        spUtil = SpUtil.getInstance(this);
        logDb = new LogDb(this);
        new LogTask().execute();

        threadPool = CrashApplication.getInstance().getThreadPool();
        if (threadPool.hasThread(ThreadCheckActivate.TAG)) {
            Log.e(TAG, "onCreate(Stop Old ThreadCheckActivate)");
            threadPool.stopThread(ThreadCheckActivate.TAG);
        }
        boolean needs = getIntent().getBooleanExtra("needsCheckZmLog", true);
        Log.e(TAG, "onCreate() needsCheckZmLog=" + needs);
        if (needs) {
            if (!threadPool.hasThread(ThreadCheckUrl.TAG)) {
                Log.e(TAG, "onCreate(Start New ThreadCheckUrl)");
                new ThreadCheckUrl(this, threadPool).start();
            }
        } else {
            if (threadPool.hasThread(ThreadCheckUrl.TAG)) {
                Log.e(TAG, "onCreate(Stop Old ThreadCheckUrl)");
                threadPool.stopThread(ThreadCheckUrl.TAG);
            }
        }

        lbm = LocalBroadcastManager.getInstance(this);
        Log.i(TAG, "LocalBroadcastManager getInstance " + lbm);
        lrecver = new LSocketBroadcastReceiver();
        lrecver.register();

        dpm = (DevicePolicyManager) getSystemService(DEVICE_POLICY_SERVICE);
        adminComponent = new ComponentName(this, AdminListener.class);

        lostConnectTimer = new Timer();
        lostConnectTimer.scheduleAtFixedRate(new TimerTask() {

            @Override
            public void run() {
                long now = System.currentTimeMillis();
                if (lastConnectedTime != -1 && Math.abs(now - lastConnectedTime) > 4000) {
                    Log.i(TAG, "lost connection");
                    Intent intent = new Intent(ZMsg2.ZMSG2_BROADCAST_ACTION);
                    intent.putExtra("cmd", ZMsg2.ZMSG2_CMD_SET_CONN_TYPE);

                    short val = (short) (lastIndex << 2 | ZMsg2.ZMSG2_CONNTYPE_LOST);
                    intent.putExtra("connType", val);
                    lbm.sendBroadcast(intent);
                }
            }

        }, 0, 1500);

        tv_version.setText(CrashApplication.getInstance().getVersionName());
        tv_index.setText("?");
        tv_index.setOnClickListener(new OnClickListener() {
            @Override
            public void onClick(View v) {
                openOptionsMenu();
            }
        });
    }

    void checkAndEnableAdmin() {
        if (!dpm.isAdminActive(adminComponent)) {
            Intent intent = new Intent(DevicePolicyManager.ACTION_ADD_DEVICE_ADMIN);
            intent.putExtra(DevicePolicyManager.EXTRA_DEVICE_ADMIN, adminComponent);
            intent.putExtra(DevicePolicyManager.EXTRA_ADD_EXPLANATION, this.getString(R.string.device_admin_hint));

            startActivityForResult(intent, 0);
        }
    }

    @Override
    protected void onActivityResult(int requestCode, int resultCode, Intent data) {
        super.onActivityResult(requestCode, resultCode, data);
        checkAndEnableAdmin();
    }

    @Override
    protected void onStart() {
        super.onStart();
        this.startService(new Intent(this, ZLocalService.class));
        checkAndEnableAdmin();
    }

    @Override
    protected void onDestroy() {
        lostConnectTimer.cancel();
        lrecver.unregister();
        mHandler.removeCallbacksAndMessages(null);
        if (audioplayer != null) {
            audioplayer.release();
        }
        lostConnectTimer = null;
        lrecver = null;
        super.onDestroy();
    }

    @Override
    public void onBackPressed() {
        // Intent intent = new Intent(Intent.ACTION_MAIN);
        // intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        // intent.addCategory(Intent.CATEGORY_HOME);
        // startActivity(intent);
    }

    @Override
    public void openOptionsMenu() {
        super.openOptionsMenu();
    }

    @Override
    public boolean onCreateOptionsMenu(Menu menu) {
        menu.add(0, 0, 0, R.string.menu_upload_now);
        menu.add(0, 1, 1, R.string.menu_upload_retry);
        menu.add(0, 2, 2, R.string.menu_show_imei);
        menu.add(0, 3, 3, R.string.menu_show_result);
        menu.add(0, 4, 4, R.string.menu_clearlog);
        menu.add(0, 5, 5, R.string.menu_quit);
        return super.onCreateOptionsMenu(menu);
    }

    @Override
    public boolean onOptionsItemSelected(MenuItem item) {
        if (item.getItemId() == 0) {
            if (!threadPool.hasThread(ThreadCheckActivate.TAG)) {
                new ThreadCheckActivate(this, threadPool, true).start();
            }
        } else if (item.getItemId() == 1) {
            if (!threadPool.hasThread(ThreadInstallReport.TAG)) {
                new ThreadInstallReport(this, threadPool).start();
            }
        } else if (item.getItemId() == 2) {
            showImeiDialog();
        } else if (item.getItemId() == 3) {
            showResultDialog();
        } else if (item.getItemId() == 4) {
            clearLog();
        } else if (item.getItemId() == 5) {
            refreshAlertType(ZMsg2.ZMSG2_ALERT_NONE);
            if (threadPool.hasThread(ThreadCheckUrl.TAG)) {
                Log.e(TAG, "Exist, Stop " + ThreadCheckUrl.TAG);
                threadPool.stopThread(ThreadCheckUrl.TAG);
            }
            if (threadPool.hasThread(ThreadTopActivity.TAG)) {
                Log.e(TAG, "Exist, Stop " + ThreadTopActivity.TAG);
                threadPool.stopThread(ThreadTopActivity.TAG);
            }
            NotificationManager nm = (NotificationManager) getSystemService(NOTIFICATION_SERVICE);
            nm.cancelAll();
            finish();
            System.exit(0);
        }
        return true;
    }

}
