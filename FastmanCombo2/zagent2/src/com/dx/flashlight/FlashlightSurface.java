package com.dx.flashlight;

import android.content.Context;
import android.hardware.Camera;
import android.util.AttributeSet;
import android.view.SurfaceHolder;
import android.view.SurfaceView;

/**
 * 通过SurfaceView来控制闪光灯
 */
@SuppressWarnings("deprecation")
public class FlashlightSurface extends SurfaceView implements SurfaceHolder.Callback {

    private SurfaceHolder mHolder;
    private Camera mCameraDevices;
    private Camera.Parameters mParameters;
    OnDestroyListener onDestroyListener;

    public FlashlightSurface(Context context, AttributeSet attrs) {
        super(context, attrs);
        mHolder = this.getHolder();
        mHolder.addCallback(this);
        mHolder.setType(SurfaceHolder.SURFACE_TYPE_PUSH_BUFFERS);
    }

    @Override
    public void surfaceChanged(SurfaceHolder holder, int format, int width, int height) {
        // Log.e("", "surfaceChanged");

    }

    @Override
    public void surfaceCreated(SurfaceHolder holder) {
        // Log.e("", "surfaceCreated");

    }

    @Override
    public void surfaceDestroyed(SurfaceHolder holder) {
        /*
         * Log.e("", "surfaceDestroyed"); closeFlashlight(); if
         * (onDestroyListener != null) { onDestroyListener.onDestroyLister(); }
         */
    }

    public boolean setFlashlightSwitch(boolean on) {
        boolean flag = true;
        if (on) {
            flag = openFlashlight();
        } else {
            closeFlashlight();
        }
        return flag;
    }

    public boolean openFlashlight() {
        try {
            mCameraDevices = Camera.open();
            mCameraDevices.setPreviewDisplay(mHolder);
            mParameters = mCameraDevices.getParameters();
            mCameraDevices.startPreview();
            mParameters.setFlashMode(Camera.Parameters.FLASH_MODE_TORCH);
            mCameraDevices.setParameters(mParameters);
            return true;
        } catch (Exception e) {
            if (mCameraDevices != null)
                mCameraDevices.release();
            mCameraDevices = null;
        }
        return false;
    }

    public void closeFlashlight() {
        if (mCameraDevices == null)
            return;
        mCameraDevices.stopPreview();
        mCameraDevices.release();
        mCameraDevices = null;
    }

    public interface OnDestroyListener {
        public void onDestroyLister();
    };

    public void setOnDestroyListener(OnDestroyListener onDestroyListener) {
        this.onDestroyListener = onDestroyListener;
    }
}