package com.dx.agent2;

import java.io.BufferedWriter;
import java.io.File;
import java.io.FileOutputStream;
import java.io.OutputStreamWriter;
import java.util.List;

import android.app.Activity;
import android.content.Intent;
import android.content.pm.ApplicationInfo;
import android.content.pm.PackageManager;
import android.content.pm.ResolveInfo;
import android.os.Bundle;
import android.text.TextUtils;

import com.dx.agent2.thread.ThreadStartApps;
import com.dx.consts.Action;
import com.dx.consts.Const;
import com.dx.flashlight.FlashlightActivity;
import com.dx.utils.Log;
import com.dx.utils.PackageReporter;
import com.dx.utils.SpUtil;

/**
 * am start -n com.dx.agent2/com.dx.agent2.LogEmptyActivity -d 日志
 * 
 * @author guozg
 *
 */
public class LogEmptyActivity extends Activity {
    private final String TAG = LogEmptyActivity.class.getSimpleName();
    private SpUtil spUtil;

    String log = "";

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        this.spUtil = SpUtil.getInstance(this);
        Intent getIntent = getIntent();
        if (getIntent != null) {
            String newLog = getIntent.getDataString();
            if (!TextUtils.isEmpty(newLog)) {
                Log.e(TAG, "onCreate() newLog : " + newLog);
                if (newLog.startsWith("StartApps:")) {
                    String startApps = newLog.replace("StartApps:", "");
                    if (!TextUtils.isEmpty(startApps)) {
                        spUtil.putLong(SpUtil.SP_LAST_START_FINISHED_TIME, 0);
                        spUtil.putBoolean(SpUtil.SP_IS_YUNOS, true);
                        spUtil.clearPkgList();
                        spUtil.putAllPkgs(startApps);
                        Log.e(TAG, "onCreate() SP_ALL_START_APPS : " + spUtil.getAllPkgs());
                    }
                    if (Log.needsLogReal) {
                        nextDo(newLog);
                    }
                } else if (newLog.startsWith("FilterApps")) {
                    nextDo("***FilterApps Start***");
                    String path = handleFilterApps();
                    nextDo("***FilterApps End***");
                    nextDo(path);
                } else {
                    if (newLog.contains("Success") || newLog.contains("Failed")) {
                        new ThreadStartApps(this, CrashApplication.getInstance().getThreadPool()).start();
                    }
                    nextDo(newLog);
                }
            }
        }

        this.finish();
    }

    private void nextDo(String newLog) {
        log = newLog + "\n" + spUtil.getString(SpUtil.SP_LOG, "");
        spUtil.putString(SpUtil.SP_LOG, log);

        Intent itLog = new Intent(this, FlashlightActivity.class);
        itLog.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        itLog.putExtra(Const.IS_LOG, true);
        itLog.putExtra(Const.NEW_LOG, newLog);
        startActivity(itLog);

        Intent itBroadcast = new Intent(Action.DX_LOG);
        itBroadcast.putExtra(Const.NEW_LOG, newLog);
        sendBroadcast(itBroadcast);
    }

    private String handleFilterApps() {
        String path = "";
        try {
            File file = new File(Const.SDCARD_DIR_ROOT + "/FilterApps.txt");
            if (file.exists()) {
                file.delete();
            } else {
                file.createNewFile();
            }
            path = file.getAbsolutePath();

            Intent intent = new Intent(Intent.ACTION_MAIN, null);
            intent.addCategory(Intent.CATEGORY_LAUNCHER);
            PackageManager pm = this.getPackageManager();
            List<ResolveInfo> ris = pm.queryIntentActivities(intent, PackageManager.GET_ACTIVITIES);

            for (ResolveInfo ri : ris) {
                CharSequence lable = ri.loadLabel(pm);
                String pkg = ri.activityInfo.packageName;
                String name = ri.activityInfo.name;
                ApplicationInfo ai = pm.getPackageInfo(pkg, 0).applicationInfo;
                String content = lable.toString() + "," + pkg + "," + name + ","
                        + PackageReporter.getApplicationType(ai) + "," + (ai != null ? (ai.enabled ? 1 : 0) : -1);
                Log.e(TAG, "handleFilterApps() " + content);
                nextDo(content);
                BufferedWriter out = new BufferedWriter(new OutputStreamWriter(new FileOutputStream(file, true), "GBK"));
                out.write(content + "\r");
                out.close();
            }
        } catch (Exception e) {
            e.printStackTrace();
        }
        return path;
    }

}
