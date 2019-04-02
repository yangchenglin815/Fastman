package com.dx.core;

import android.content.Context;

public class DxCore {

    private int mPtr = 0;

    private static DxCore mInstance = null;

    public static synchronized DxCore getInstance(Context context) {
        if (mInstance == null) {
            mInstance = new DxCore();
            if (!mInstance.setContext(mInstance.mPtr, context)) {
                mInstance = null;
            }
        }
        return mInstance;
    }

    private DxCore() {
        System.loadLibrary("dxcore");
        mPtr = create();
    }

    public String getFingerPrint(String str) {
        byte[] data = str.getBytes();
        return getFingerPrint(mPtr, data, 0, data.length);
    }

    public String getFingerPrint(byte[] data, int pos, int len) {
        return getFingerPrint(mPtr, data, pos, len);
    }

    public String encode(String in) {
        byte[] inData = in.getBytes();
        byte[] outData = new byte[inData.length + 2];
        encode(inData, outData, 0, inData.length);
        return toHexString(outData);
    }

    public String decode(String in) {
        byte[] inData = hexStringToByteArray(in);
        byte[] outData = new byte[inData.length - 2];
        decode(inData, outData, 0, inData.length);
        return new String(outData, 0, outData.length);
    }

    public byte[] encodeAsByteArray(String in) {
        byte[] inData = in.getBytes();
        byte[] outData = new byte[inData.length + 2];

        encode(inData, outData, 0, inData.length);
        return outData;
    }

    public String decodeAsString(byte[] inData) {
        byte[] outData = new byte[inData.length - 2];

        decode(inData, outData, 0, inData.length);
        return new String(outData, 0, outData.length);
    }

    public int encode(byte[] in, byte[] out, int pos, int len) {
        return encode(mPtr, in, out, pos, len);
    }

    public int decode(byte[] in, byte[] out, int pos, int len) {
        return decode(mPtr, in, out, pos, len);
    }

    private native int create();

    private native void destroy(int ptr);

    private native boolean setContext(int ptr, Object context);

    private native String getFingerPrint(int ptr, byte[] data, int pos, int len);

    public native String getDllVersion();

    private native int encode(int ptr, byte[] in, byte[] out, int pos, int len);

    private native int decode(int ptr, byte[] in, byte[] out, int pos, int len);

    private static char hexDigits[] = { '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'a', 'b', 'c', 'd', 'e', 'f' };

    private static String toHexString(byte[] bytes) {
        StringBuffer stringbuffer = new StringBuffer(2 * bytes.length);
        for (int l = 0; l < bytes.length; l++) {
            char c0 = hexDigits[(bytes[l] & 0xf0) >> 4];
            char c1 = hexDigits[bytes[l] & 0xf];
            stringbuffer.append(c0);
            stringbuffer.append(c1);
        }
        return stringbuffer.toString();
    }

    private static int toByte(char c) {
        byte b = (byte) "0123456789abcdef".indexOf(c);
        return b;
    }

    private static byte[] hexStringToByteArray(String hex) {
        int len = (hex.length() / 2);
        byte[] result = new byte[len];
        char[] achar = hex.toCharArray();
        for (int i = 0; i < len; i++) {
            int pos = i * 2;
            result[i] = (byte) (toByte(achar[pos]) << 4 | toByte(achar[pos + 1]));
        }
        return result;
    }
}
