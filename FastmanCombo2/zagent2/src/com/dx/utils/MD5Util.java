package com.dx.utils;

import java.io.FileInputStream;
import java.security.MessageDigest;
import java.util.Locale;

import android.text.TextUtils;

public class MD5Util {
    public static final String TAG = MD5Util.class.getSimpleName();

    private static String getMd5(byte[] data) {
        try {
            MessageDigest messagedigest = MessageDigest.getInstance("MD5");
            messagedigest.update(data);
            return HexUtil.toHexString(messagedigest.digest());
        } catch (Exception e) {
            e.printStackTrace();
        }
        return "";
    }

    public static String getStringMd5(String str, boolean isUpperCase) {
        String md5 = "";
        if (!TextUtils.isEmpty(str)) {
            md5 = getMd5(str.getBytes());
            if (isUpperCase) {
                md5 = md5.toUpperCase(Locale.getDefault());
            }
        }
        return md5;
    }

    /**
     * 获取文件的MD5值
     * 
     * @param path
     *            文件路径
     * @param isUpperCase
     *            是否大写
     * @return
     */
    public static String getFileMd5(String path, boolean isUpperCase) {
        String md5 = "";
        if (!TextUtils.isEmpty(path)) {
            try {
                MessageDigest messagedigest = MessageDigest.getInstance("MD5");
                byte[] buf = new byte[4096];
                int n = 0;
                FileInputStream fis = new FileInputStream(path);
                while ((n = fis.read(buf, 0, 4096)) > 0) {
                    messagedigest.update(buf, 0, n);
                }
                fis.close();
                md5 = HexUtil.toHexString(messagedigest.digest());
                if (isUpperCase) {
                    md5 = md5.toUpperCase(Locale.getDefault());
                }
            } catch (Exception e) {
                e.printStackTrace();
            }
        }
        return md5;
    }

    /**
     * md5加密产生，产生128位（bit）的Mac<br>
     * 将128bit Mac转换成16进制代码
     * 
     * @param str
     * @param isUpperCase
     *            是否大写
     * @return
     */
    public static String MD5Encode(String str, boolean isUpperCase) {
        Log.v(TAG, "encode before : " + str);
        String result = "";
        try {
            MessageDigest md5 = MessageDigest.getInstance("MD5");
            md5.update(str.getBytes("UTF8"));
            byte[] bytes = md5.digest();
            result = HexUtil.toHexString(bytes);
            if (isUpperCase) {
                result = result.toUpperCase(Locale.getDefault());
            }
        } catch (Exception e) {
            e.printStackTrace();
        }
        Log.v(TAG, "encode after : " + result);
        return result;
    }
}
