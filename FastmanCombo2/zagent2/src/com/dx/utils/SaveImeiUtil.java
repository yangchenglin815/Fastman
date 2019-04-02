package com.dx.utils;

import java.io.BufferedReader;
import java.io.BufferedWriter;
import java.io.File;
import java.io.FileOutputStream;
import java.io.InputStreamReader;
import java.io.OutputStreamWriter;
import java.io.PrintWriter;
import java.util.Random;

import android.content.Context;

import com.dx.consts.Const;

public class SaveImeiUtil {
    public static final String TAG = SaveImeiUtil.class.getSimpleName();

    public static void save(Context context) {
        String imei = "";
        imei = IPhoneSubInfoUtil.getSmallestImei(context);
        if (null != imei) {
            saveImei(imei, "imei");
        }

        String allImei = "";
        allImei = IPhoneSubInfoUtil.getAllImei(context, false);
        if (null != allImei) {
            saveImei(allImei, "imei_All");
        }
    }

    static void saveImei(String imei, String name) {
        try {
            File file = new File(Const.SDCARD_DIR_ROOT + "/" + name + ".txt");
            if (file.exists()) {
                file.delete();
            } else {
                file.createNewFile();
            }
            BufferedWriter out = new BufferedWriter(new OutputStreamWriter(new FileOutputStream(file, true), "GBK"));
            out.write(imei);
            out.close();
            Log.e(TAG, "saveImei() path = " + file.getAbsolutePath() + " \n imei = " + imei);
        } catch (Exception e) {
            e.printStackTrace();
        }
    }

    static void saveImei(Context ct, String imei, String name) {
        FileOutputStream outStream;
        try {
            outStream = ct.openFileOutput(name + ".txt", Context.MODE_PRIVATE);
            outStream.write(imei.getBytes());
            outStream.close();
            // File file = ct.getFilesDir();
            // String path = file.getAbsolutePath();
            Log.e(TAG, "saveImei() path = " + Const.SDCARD_DIR_ROOT + "/" + name + ".txt" + " \n imei = " + imei);
            doExec("chmod 666 " + Const.SDCARD_DIR_ROOT + "/" + "imei" + ".txt");
        } catch (Exception e) {
        }

    }

    /**
     * 执行adb命令返回值
     * 
     * @param cmd
     * @return
     */
    public static String doExec(String cmd) {
        String s = "";
        String randomNum = getRandomNum();

        String myreadline;
        ProcessBuilder pb = new ProcessBuilder("/system/bin/sh");
        pb.directory(new File("/"));
        try {
            Process proc = pb.start();
            BufferedReader in = new BufferedReader(new InputStreamReader(proc.getInputStream()));
            PrintWriter out = new PrintWriter(new BufferedWriter(new OutputStreamWriter(proc.getOutputStream())), true);
            out.println(cmd + " 2>&1");
            out.println("echo " + randomNum);

            while ((myreadline = in.readLine()) != null) {
                s = s + myreadline;
                if (myreadline.compareTo(randomNum) == 0) {
                    break;
                }
            }
            in.close();
            out.close();
            proc.destroy();
        } catch (Exception e) {
            e.printStackTrace();
        }
        return s;

    }

    /**
     * 获取随随机数
     */
    private static String getRandomNum() {
        String number = "";
        Random rd = new Random();
        while (number.length() != 10) {
            String rn = rd.nextInt(10) + "";
            if (number.indexOf(rn) == -1)
                number += rn;
        }
        return number;
    }

}
