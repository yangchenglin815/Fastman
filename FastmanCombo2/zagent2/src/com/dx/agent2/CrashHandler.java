package com.dx.agent2;

import java.io.File;
import java.io.FileNotFoundException;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.PrintWriter;
import java.io.StringWriter;
import java.io.Writer;
import java.lang.Thread.UncaughtExceptionHandler;
import java.text.SimpleDateFormat;
import java.util.Calendar;
import java.util.GregorianCalendar;
import java.util.HashMap;
import java.util.Map;

import android.annotation.SuppressLint;
import android.app.ActivityManager;
import android.content.Context;
import android.content.pm.PackageInfo;
import android.content.pm.PackageManager;
import android.os.Build;
import android.os.Looper;
import android.text.format.Formatter;
import android.widget.Toast;

import com.dx.consts.Const;
import com.dx.utils.Log;

/**
 * UncaughtException处理类,当程序发生Uncaught异常的时候,由该类来接管程序,并记录发送错误报告。
 */
public class CrashHandler implements UncaughtExceptionHandler {
    final String TAG = CrashHandler.class.getSimpleName();
    private Thread.UncaughtExceptionHandler mDefaultHandler;// 系统默认的UncaughtException处理类
    private static CrashHandler mInstance;// CrashHandler实例
    private Context mContext;// 程序的Context对象
    private Map<String, String> info = new HashMap<String, String>();// 用来存储设备信息和异常信息

    /** 保证只有一个CrashHandler实例 */
    private CrashHandler() {
    }

    /** 获取CrashHandler实例 ,单例模式 */
    public static CrashHandler getInstance() {
        if (mInstance == null) {
            mInstance = new CrashHandler();
        }
        return mInstance;
    }

    /**
     * 初始化
     * 
     * @param context
     */
    public void init(Context context) {
        mContext = context;
        mDefaultHandler = Thread.getDefaultUncaughtExceptionHandler();// 获取系统默认的UncaughtException处理器
        Thread.setDefaultUncaughtExceptionHandler(this);// 设置该CrashHandler为程序的默认处理器
    }

    /**
     * 当UncaughtException发生时会转入该重写的方法来处理
     */
    public void uncaughtException(Thread thread, Throwable ex) {
        if (!handleException(ex) && mDefaultHandler != null) {
            // 如果自定义的没有处理则让系统默认的异常处理器来处理
            mDefaultHandler.uncaughtException(thread, ex);
        } else {
            try {
                Thread.sleep(10000);// 如果处理了，让程序继续运行5秒再退出，保证文件保存并上传到服务器
            } catch (InterruptedException e) {
                e.printStackTrace();
            }
        }
        // 结束进程
        System.exit(0);
    }

    /**
     * 自定义错误处理,收集错误信息 发送错误报告等操作均在此完成.
     * 
     * @param ex
     *            异常信息 如果处理了该异常信息返回 true;否则返回false。
     */
    public boolean handleException(Throwable ex) {
        if (ex == null)
            return false;
        new Thread() {
            public void run() {
                Looper.prepare();
                Toast.makeText(mContext, "很抱歉,精灵出现异常,即将退出", Toast.LENGTH_SHORT).show();
                Looper.loop();
            }
        }.start();
        Log.e(TAG, "handleException(很抱歉,精灵出现异常,即将退出)");
        // 收集设备参数信息
        collectDeviceInfo(mContext);
        // 保存日志文件
        saveCrashInfo2File(ex);
        return true;
    }

    /**
     * 收集设备参数信息
     * 
     * @param context
     */
    public void collectDeviceInfo(Context context) {
        try {
            PackageManager pm = context.getPackageManager();// 获得包管理器
            PackageInfo pi = pm.getPackageInfo(context.getPackageName(), PackageManager.GET_ACTIVITIES);// 得到该应用的信息，即主Activity
            if (pi != null) {
                String versionName = pi.versionName == null ? "null" : pi.versionName;
                String versionCode = pi.versionCode + "";
                info.put("AppInfo", pi.applicationInfo.loadLabel(pm) + " , package:" + context.getPackageName()
                        + " , version:" + versionCode + "_V" + versionName);
                info.put("DeviceInfo", Build.MANUFACTURER + " " + Build.MODEL + " " + Build.VERSION.RELEASE);
            }

            ActivityManager am = (ActivityManager) mContext.getSystemService(Context.ACTIVITY_SERVICE);
            ActivityManager.MemoryInfo mi = new ActivityManager.MemoryInfo();
            am.getMemoryInfo(mi);
            info.put("AvailMemoryInfo", Formatter.formatFileSize(mContext, mi.availMem));
            info.put("HeapGrowthLimit", am.getMemoryClass() + "MB");
        } catch (Exception e) {
            e.printStackTrace();
        }
    }

    private String saveCrashInfo2File(Throwable ex) {
        StringBuffer sb = new StringBuffer();
        for (Map.Entry<String, String> entry : info.entrySet()) {
            String key = entry.getKey();
            String value = entry.getValue();
            sb.append(key + "=" + value + "\r\n");
        }
        Writer writer = new StringWriter();
        PrintWriter pw = new PrintWriter(writer);
        ex.printStackTrace(pw);
        Throwable cause = ex.getCause();
        // 循环着把所有的异常信息写入writer中
        while (cause != null) {
            cause.printStackTrace(pw);
            cause = cause.getCause();
        }
        pw.close();// 记得关闭
        String result = writer.toString();
        sb.append(result);
        // 保存文件
        String time = getCurrentDate();
        String fileName = "log-" + time + ".txt";
        String logFilePath = Const.SDCARD_DIR_LOG;
        try {
            File dir = new File(logFilePath);
            if (!dir.exists())
                dir.mkdirs();
            FileOutputStream fos = new FileOutputStream(new File(dir, fileName));
            fos.write(sb.toString().getBytes());
            fos.close();
            return fileName;
        } catch (FileNotFoundException e) {
            e.printStackTrace();
        } catch (IOException e) {
            e.printStackTrace();
        } finally {
            ex.printStackTrace();
        }
        return null;
    }

    /**
     * 获取表示当前日期时间的字符串 格式化字符串，如："yyyy-MM-dd HHmmss"
     * 
     * @return String String类型的当前日期时间
     */
    @SuppressLint("SimpleDateFormat")
    public static String getCurrentDate() {
        String format = "yyyy-MM-dd HHmmss";
        String time = null;
        try {
            SimpleDateFormat sdf = new SimpleDateFormat(format);
            Calendar c = new GregorianCalendar();
            time = sdf.format(c.getTime());
        } catch (Exception e) {
            e.printStackTrace();
        }
        return time;

    }
}
