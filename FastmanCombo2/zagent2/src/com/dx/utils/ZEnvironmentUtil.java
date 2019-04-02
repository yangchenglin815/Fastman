package com.dx.utils;

import java.io.BufferedReader;
import java.io.File;
import java.io.FileInputStream;
import java.io.IOException;
import java.io.InputStreamReader;
import java.util.ArrayList;
import java.util.List;

import android.os.Environment;

import com.dx.consts.Const;

public class ZEnvironmentUtil {
    public static final String TAG = ZEnvironmentUtil.class.getSimpleName();

    /**
     * @author fay
     * @return all mounted storage directories, list.size() might be 0
     */
    public static List<String> getMountedStorageDirs() {
        List<String> ret = new ArrayList<String>();
        try {
            FileInputStream fis = new FileInputStream("/proc/mounts");
            // Process p = Runtime.getRuntime().exec("mount");
            // InputStream fis = p.getInputStream();
            BufferedReader br = new BufferedReader(new InputStreamReader(fis));
            String line = null;
            String[] array = null;
            String mountpoint = null;
            String type = null;
            while ((line = br.readLine()) != null) {
                // spaces are essential and tricky
                if (line.contains(" vfat ") || line.contains(" fuse ") || line.contains(" sdcardfs ")) {
                    array = line.split(" ");
                    mountpoint = array[1];
                    type = array[2];
                    // strict detection
                    if (type.equals("vfat") || type.equals("fuse")) {
                        if (mountpoint.contains("asec") || mountpoint.contains("firmware")) {
                            continue;
                        }
                        Log.e("", "found " + mountpoint);
                        ret.add(mountpoint);
                    }
                }
            }
            br.close();
            fis.close();
        } catch (Exception e) {
            e.printStackTrace();
        }
        return ret;
    }

    /**
     * @author gaogm
     * @return all mounted storage directories, list.size() might be 0
     */
    public static List<String> getAllMountedStorageDirs() {
        List<String> ret = getMountedStorageDirs();
        if (Environment.MEDIA_MOUNTED.equals(Environment.getExternalStorageState())) {
            String externaPath = Environment.getExternalStorageDirectory().getPath();
            Log.i(TAG, "getAllMountedStorageDirs externaFile: " + externaPath);
            if (!ret.contains(externaPath)) {
                ret.add(externaPath);
            }
        }
        return ret;
    }

    /**
     * @author guozg
     * @param dirName
     *            指定目录名称
     * @return 返回指定的目录绝对路径： <br>
     *         Environment.getExternalStorageDirectory()+"/"+dirPath;<br>
     *         返回值为空，代表目录无法创建
     */
    public static String getExternalDirectory(String dirName) {
        String defDir = Environment.getExternalStorageDirectory() + "/" + dirName;
        File f = new File(defDir);
        if (f.exists() && f.isDirectory()) {
            return f.toString();
        } else {
            f.mkdirs();
            if (f.exists() && f.isDirectory()) {
                return defDir;
            } else {
                return "";
            }
        }
    }

    /** 判断手机外部存储是否可用 **/
    public static boolean externalDirIsCanUse() {
        boolean isCanUse = false;
        String state = Environment.getExternalStorageState();
        if (Environment.MEDIA_MOUNTED.equals(state)) {
            File tag = new File(Environment.getExternalStorageDirectory() + "/tag.tag");
            if (tag.exists()) {
                isCanUse = true;
            } else {
                try {
                    isCanUse = tag.createNewFile();
                } catch (IOException e) {
                    isCanUse = false;
                }
            }
        }
        Log.v(TAG, "externalDirIsCanUse()" + isCanUse);
        return isCanUse;
    }

    /** 判断根目录存储是否可用 **/
    public static boolean rootPathIsCanUse() {
        boolean isCanUse = false;
        String state = Environment.getExternalStorageState();
        if (Environment.MEDIA_MOUNTED.equals(state)) {
            File tag = new File(Const.SDCARD_DIR_ROOT + "/tag.tag");
            if (tag.exists()) {
                isCanUse = true;
            } else {
                try {
                    isCanUse = tag.createNewFile();
                } catch (IOException e) {
                    isCanUse = false;
                }
            }
        }
        Log.v(TAG, "rootPathIsCanUse()" + isCanUse);
        return isCanUse;
    }
}
