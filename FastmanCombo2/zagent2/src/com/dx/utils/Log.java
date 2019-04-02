package com.dx.utils;

import java.io.BufferedWriter;
import java.io.File;
import java.io.FileOutputStream;
import java.io.OutputStreamWriter;
import java.io.PrintWriter;
import java.io.StringWriter;
import java.io.UnsupportedEncodingException;
import java.lang.reflect.Field;
import java.text.SimpleDateFormat;
import java.util.Calendar;
import java.util.List;
import java.util.Locale;

import android.annotation.SuppressLint;

import com.dx.consts.Const;

public class Log {
    public static final String TAG = "Log";
    public static final boolean needsLogCrash = true;
    public static final boolean needsLogReal = false;
    public static final boolean needsLogFile = true;
    public static final boolean needsStartAppsTiming = true;// 后台定时启动推广应用的服务

    public static void hex(String TAG, byte[] data, int offset, int count) {
        if (needsLogReal) {
            String line = "** hex data " + count + " bytes **\n";
            for (int i = offset; i < count; i++) {
                int pos = (i - offset + 1);

                line += String.format("%02X ", data[i]);

                if (pos % 16 == 0) {
                    line += "\n";
                } else if (pos % 4 == 0) {
                    line += " ";
                }
            }
            android.util.Log.w(TAG, line);
        }
    }

    public static void printStackTrace(Throwable t) {
        if (needsLogReal) {
            t.printStackTrace();
        }
    }

    public static String getStackTrace(Throwable e) {
        StringWriter sw = new StringWriter();
        PrintWriter pw = new PrintWriter(sw);
        e.printStackTrace(pw);
        return sw.toString();
    }

    public static void printStackTrace(Exception e) {
        if (needsLogReal) {
            e.printStackTrace();
        }
    }

    public static String getStackTrace(Exception e) {
        StringWriter sw = new StringWriter();
        PrintWriter pw = new PrintWriter(sw);
        e.printStackTrace(pw);
        return sw.toString();
    }

    public static void i(String tag, String msg) {
        if (needsLogReal) {
            android.util.Log.i(tag, msg);
        }
    }

    public static void w(String tag, String msg) {
        if (needsLogReal) {
            android.util.Log.w(tag, msg);
        }
    }

    public static void d(String tag, String msg) {
        if (needsLogReal) {
            android.util.Log.d(tag, msg);
        }
    }

    public static void e(String tag, String msg) {
        if (needsLogReal) {
            android.util.Log.e(tag, msg);
        }
    }

    public static void v(String tag, String msg) {
        if (needsLogReal) {
            android.util.Log.v(tag, msg);
        }
    }

    public static void dumpObject(String tag, Object obj) {
        if (needsLogReal) {
            Field[] fields = obj.getClass().getFields();
            for (Field f : fields) {
                try {
                    Object o = f.get(obj);
                    if (o instanceof List) {
                        android.util.Log.i(tag, f.getName() + "(list size = " + ((List<?>) o).size() + "):");
                        for (Object l : (List<?>) o) {
                            dumpObject(tag, l);
                        }
                    } else {
                        android.util.Log.i(tag, f.getName() + " : " + f.get(obj));
                    }
                } catch (Exception e) {
                    e.printStackTrace();
                }
            }
        }
    }

    public static void saveException2File(String tag, Exception e) {
        Log.saveLog2File(tag, e.toString(), "Exception");
    }

    @SuppressLint("SimpleDateFormat")
    public static void saveLog2File(String tag, String content, String filename) {
        e(tag, content);
        if (needsLogFile) {
            try {
                File dir = new File(Const.SDCARD_DIR_LOG);
                if (!dir.exists()) {
                    dir.mkdirs();
                }

                File file = new File(Const.SDCARD_DIR_LOG + "/" + filename + ".txt");
                if (file.length() > 1048576) {// 文件大小>1MB
                    file.delete();
                }
                if (!file.exists()) {
                    file.createNewFile();
                }

                SimpleDateFormat sdf = new SimpleDateFormat("yyyy-MM-dd HH:mm:ss");
                String time = sdf.format(Calendar.getInstance(Locale.CHINA).getTime());
                if (!Log.needsLogReal) {// 对日志内容进行加密
                    time = encrypt(time);
                    content = encrypt(content);
                    tag = encrypt(tag);
                }
                BufferedWriter out = new BufferedWriter(new OutputStreamWriter(new FileOutputStream(file, true), "GBK"));
                out.write(time + "\r\n");
                out.write(tag + "\r\n");
                out.write(content + "\r\n\n");
                out.close();
            } catch (Exception e) {
                e.printStackTrace();
            }
        }
    }

    /**
     * 加密字符串
     * 
     * @param str
     * @return
     */
    public static String encrypt(String str) {
        int k = 0;
        byte[] b = str.getBytes();
        for (int i = 0; i < b.length; i++) {
            k = k + i;
            k = k % 8;
            b[i] = (byte) (b[i] ^ ((byte) k));
        }
        try {
            return new String(b, "GBK");
        } catch (UnsupportedEncodingException e) {
            e.printStackTrace();
        }
        return "";
    }
}
