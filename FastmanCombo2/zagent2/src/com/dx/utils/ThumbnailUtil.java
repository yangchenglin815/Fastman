package com.dx.utils;

import java.io.File;

import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.media.ThumbnailUtils;
import android.text.TextUtils;

public class ThumbnailUtil {
    static final String TAG = ThumbnailUtil.class.getSimpleName();

    /**
     * 获取本地图片文件缩略图，根据传入的高宽进行缩放
     * 
     * @param path
     * @param height
     * @param width
     * @return
     */
    public static Bitmap getLocalImageThumbnailScaleByHW(String path, int height, int width) {
        Bitmap bitmap = null;
        if (TextUtils.isEmpty(path) || !new File(path).exists()) {
            return bitmap;
        }
        BitmapFactory.Options opts = new BitmapFactory.Options();
        opts.inJustDecodeBounds = true;
        // 获取这个图片的宽和高，注意此处的bitmap为null
        bitmap = BitmapFactory.decodeFile(path, opts);
        opts.inJustDecodeBounds = false;// 设为 false
        // 计算缩放比
        int h = opts.outHeight;
        int w = opts.outWidth;
        int scaleHeight = h / height;
        int scaleWidth = w / width;
        int scale = 1;
        if (scaleWidth < scaleHeight) {
            scale = scaleWidth;
        } else {
            scale = scaleHeight;
        }
        if (scale <= 0) {
            scale = 1;
        }
        opts.inSampleSize = scale;
        // 重新读入图片，读取缩放后的bitmap
        bitmap = BitmapFactory.decodeFile(path, opts);
        // 利用ThumbnailUtils来创建缩略图，这里要指定要缩放哪个Bitmap对象
        if (width == 0) {
            width = w;
            height = h;
        }
        bitmap = ThumbnailUtils.extractThumbnail(bitmap, width, height, ThumbnailUtils.OPTIONS_RECYCLE_INPUT);
        return bitmap;
    }

    /**
     * 获取本地图片文件缩略图，根据文件大小进行缩放，适合客户端
     * 
     * @param path
     * @return
     */
    public static Bitmap getLocalImageThumbnailScaleBySize(String path) {
        Bitmap bitmap = null;
        File file = new File(path);
        if (TextUtils.isEmpty(path) || !file.exists()) {
            return bitmap;
        }
        try {
            BitmapFactory.Options opts = new BitmapFactory.Options();
            opts.inJustDecodeBounds = true;
            // 获取这个图片的宽和高，注意此处的bitmap为null
            bitmap = BitmapFactory.decodeFile(path, opts);
            opts.inJustDecodeBounds = false;// 设为 false
            int scale = getSampleSize(file);
            opts.inSampleSize = scale;
            // 重新读入图片，读取缩放后的bitmap
            bitmap = BitmapFactory.decodeFile(path, opts);
        } catch (OutOfMemoryError e) {
            // TODO: handle exception
        }
        return bitmap;
    }

    private static int getSampleSize(File file) {
        int sampleSize = (int) (file.length() / 400 / 1024);
        return sampleSize == 0 ? 1 : sampleSize;
    }

    /**
     * 获取本地图片文件缩略图，根据屏幕高宽动态计算
     * 
     * @param path
     * @return
     */
    public static Bitmap getLocalImageThumbnailScaleDynamic(String path) {
        Bitmap bitmap = null;
        if (TextUtils.isEmpty(path) || !new File(path).exists()) {
            return bitmap;
        }
        try {
            BitmapFactory.Options opts = new BitmapFactory.Options();
            opts.inJustDecodeBounds = true;
            // 获取这个图片的宽和高，注意此处的bitmap为null
            bitmap = BitmapFactory.decodeFile(path, opts);
            // 计算缩放比例
            opts.inJustDecodeBounds = false;// 设为 false
            int scale = calculateInSampleSize(opts, 480, 640);
            opts.inSampleSize = scale;
            // 重新读入图片，读取缩放后的bitmap
            bitmap = BitmapFactory.decodeFile(path, opts);
        } catch (OutOfMemoryError e) {
            // TODO: handle exception
        }
        return bitmap;
    }

    /**
     * 动态计算图片压缩比例
     * 
     * @param options
     * @param reqWidth
     * @param reqHeight
     * @return
     */
    private static int calculateInSampleSize(BitmapFactory.Options options, int reqWidth, int reqHeight) {
        final int height = options.outHeight;
        final int width = options.outWidth;
        int inSampleSize = 1;
        if (height > reqHeight || width > reqWidth) {
            if (width > height) {
                inSampleSize = Math.round((float) height / (float) reqHeight);
            } else {
                inSampleSize = Math.round((float) width / (float) reqWidth);
            }
            final float totalPixels = width * height;
            final float totalReqPixelsCap = reqWidth * reqHeight * 2;
            while (totalPixels / (inSampleSize * inSampleSize) > totalReqPixelsCap) {
                inSampleSize++;
            }
        }
        return inSampleSize;
    }

    /**
     * 获取本地视频文件缩略图
     * 
     * @param videoPath
     * @param height
     * @param width
     * @param kind
     * @return
     */
    public static Bitmap getLocalVideoThumbnail(String videoPath, int height, int width, int kind) {
        Bitmap bitmap = null;
        // 获取视频的缩略图
        // bitmap = ThumbnailUtils.createVideoThumbnail(videoPath, kind);
        bitmap = ThumbnailUtils.extractThumbnail(bitmap, width, height, ThumbnailUtils.OPTIONS_RECYCLE_INPUT);
        return bitmap;
    }

    /**
     * 获取本地音频文件缩略图
     * 
     * @param audioPath
     * @param height
     * @param width
     * @param kind
     * @return
     */
    public static Bitmap getLocalAudioThumbnail(String audioPath, int height, int width, int kind) {
        Bitmap bitmap = null;
        // bitmap = ThumbnailUtils.createVideoThumbnail(audioPath, kind);
        bitmap = ThumbnailUtils.extractThumbnail(bitmap, width, height, ThumbnailUtils.OPTIONS_RECYCLE_INPUT);
        return bitmap;
    }

}
