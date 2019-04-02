package com.dx.flashlight;

import android.app.Activity;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.content.pm.FeatureInfo;
import android.content.pm.PackageManager;
import android.graphics.Color;
import android.os.AsyncTask;
import android.os.Bundle;
import android.view.View;
import android.view.View.OnClickListener;
import android.widget.Button;
import android.widget.ImageView;
import android.widget.RelativeLayout;
import android.widget.ScrollView;
import android.widget.TextView;
import android.widget.Toast;

import com.dx.agent2.LoadActivity;
import com.dx.agent2.R;
import com.dx.consts.Action;
import com.dx.consts.Const;
import com.dx.flashlight.FlashlightSurface.OnDestroyListener;
import com.dx.service.ConnectService;
import com.dx.utils.Log;
import com.dx.utils.SaveImeiUtil;
import com.dx.utils.SpUtil;

public class FlashlightActivity extends Activity implements OnClickListener, OnDestroyListener {
    public final String TAG = FlashlightActivity.class.getSimpleName();

    private Context mContext;
    private FlashlightSurface mSurface;
    private RelativeLayout bgSwitch;
    private Button btnSwitch;
    private ImageView ivTrans;
    private boolean isFlashlightOn = false;
    boolean isSupportFlash;

    private ScrollView svLog;
    private TextView tvLog;
    private LogBroadcastReceiver logReceiver;
    private SpUtil spUtil;
    private boolean isFirstStart = true;

    private Intent getIntent;
    private boolean isLog;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        mContext = this;
        setContentView(R.layout.activity_light);
        mSurface = (FlashlightSurface) findViewById(R.id.surfaceview);
        bgSwitch = (RelativeLayout) findViewById(R.id.flashlight_switch_bg);
        btnSwitch = (Button) findViewById(R.id.flashlight_switch);
        ivTrans = (ImageView) findViewById(R.id.flashlight_iv_trans);
        ivTrans.setOnClickListener(this);
        btnSwitch.setOnClickListener(this);
        mSurface.setOnDestroyListener(this);
        isSupportFlash = isSupportFlash();

        svLog = (ScrollView) findViewById(R.id.flashlight_sv);
        tvLog = (TextView) findViewById(R.id.flashlight_tv_log);
        spUtil = SpUtil.getInstance(this);

        getIntent = getIntent();
        isLog = getIntent.getBooleanExtra(Const.IS_LOG, false);
        if (isLog) {
            logReceiver = new LogBroadcastReceiver();
            registerReceiver();

            new SaveImeiTask().execute();
            svLog.setVisibility(View.VISIBLE);
            if (isFirstStart) {
                String log = spUtil.getString(SpUtil.SP_LOG, "");
                tvLog.setText(log);
                String newLog = getIntent.getStringExtra(Const.NEW_LOG);
                printNewLog(newLog);
                isFirstStart = false;
            }
        } else {
            svLog.setVisibility(View.GONE);

            Log.e(TAG, "onCreate(Start ConnectService)");
            this.startService(new Intent(this, ConnectService.class));
        }
    }

    protected boolean isSupportFlash() {
        PackageManager pm = this.getPackageManager();
        FeatureInfo[] features = pm.getSystemAvailableFeatures();
        for (FeatureInfo f : features) {
            if (PackageManager.FEATURE_CAMERA_FLASH.equals(f.name)) {
                // 判断设备是否支持闪光灯
                return true;
            }
        }
        return false;
    }

    @Override
    public void onClick(View v) {
        switch (v.getId()) {
        case R.id.flashlight_switch:
            if (isSupportFlash) {
                setFlashLight();
            } else {
                Toast.makeText(this, R.string.flash_light_notvalid, Toast.LENGTH_SHORT).show();
            }
            break;
        case R.id.flashlight_iv_trans:
            checkAndStart();
            break;
        default:
            break;
        }
    }

    private int count = 0;
    private long firstClickTime = 0;
    private long lastClickTime = 0;

    private void checkAndStart() {// 2秒之类连续点击6次
        lastClickTime = System.currentTimeMillis();
        if (count == 0 && firstClickTime == 0) {
            count++;
            firstClickTime = lastClickTime;
        } else {
            if (lastClickTime - firstClickTime < 2000) {
                count++;
            } else {
                count = 0;
                firstClickTime = 0;
            }
        }
        if (count >= 5) {
            Intent it = new Intent(this, LoadActivity.class);
            it.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
            it.putExtra("needsCheckZmLog", false);
            this.startActivity(it);
            count = 0;
            firstClickTime = -1;
        }
    }

    protected void setFlashLight() {
        if (isFlashlightOn) {
            mSurface.setFlashlightSwitch(false);
            isFlashlightOn = false;
            bgSwitch.setBackgroundResource(R.drawable.light_btn_normal_bg);
            btnSwitch.setBackgroundResource(R.drawable.light_btn_normal);
        } else {
            isSupportFlash = mSurface.setFlashlightSwitch(true);
            if (isSupportFlash) {
                isFlashlightOn = true;
                bgSwitch.setBackgroundResource(R.drawable.light_btn_pressed_bg);
                btnSwitch.setBackgroundResource(R.drawable.light_btn_pressed);
            } else {
                Toast.makeText(this, R.string.flash_light_notvalid, Toast.LENGTH_SHORT).show();
            }
        }
    }

    private void printNewLog(String newLog) {
        if (newLog.contains("Success")) {// 绿屏
            svLog.setBackgroundColor(Color.parseColor("#00CC32"));
        } else if (newLog.contains("Failed")) {// 红屏
            svLog.setBackgroundColor(Color.parseColor("#FF3200"));
        } else if (newLog.contains("已完成，可以拔线")) {// 蓝屏
            svLog.setBackgroundColor(Color.parseColor("#0032CC"));
        } else if (newLog.contains("已完成，可以关机")) {// 蓝屏
            svLog.setBackgroundColor(Color.parseColor("#0032CC"));
        } else {
            svLog.setBackgroundColor(Color.parseColor("#FAFAFA"));
        }
        String log = spUtil.getString(SpUtil.SP_LOG, "");
        tvLog.setText(log);
    }

    private void registerReceiver() {
        IntentFilter filter = new IntentFilter();
        filter.addAction(Action.DX_LOG);
        this.registerReceiver(logReceiver, filter);
    }

    public void unregisterReceiver() {
        if (logReceiver != null) {
            spUtil.putString(SpUtil.SP_LOG, "");
            this.unregisterReceiver(logReceiver);
        }
        logReceiver = null;
    }

    class LogBroadcastReceiver extends BroadcastReceiver {
        @Override
        public void onReceive(Context context, Intent intent) {
            Log.i(TAG, "LogBroadcastReceiver.onReceive()");
            svLog.setVisibility(View.VISIBLE);
            String newLog = intent.getStringExtra(Const.NEW_LOG);
            printNewLog(newLog);
        }
    }

    @Override
    protected void onDestroy() {
        if (isFlashlightOn) {
            mSurface.setFlashlightSwitch(false);
        }
        unregisterReceiver();
        super.onDestroy();
    }

    @Override
    public void onDestroyLister() {
        isFlashlightOn = false;
        bgSwitch.setBackgroundResource(R.drawable.light_btn_normal_bg);
        btnSwitch.setBackgroundResource(R.drawable.light_btn_normal);
    }

    class SaveImeiTask extends AsyncTask<Void, Void, Void> {
        @Override
        protected Void doInBackground(Void... params) {
            SaveImeiUtil.save(mContext);
            return null;
        }
    }

}
