package com.dx.utils;

import java.io.ByteArrayInputStream;
import java.io.ByteArrayOutputStream;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.ObjectInputStream;
import java.io.ObjectOutputStream;

public class HexUtil {
    static final boolean needsLog = true;
    static final String uuid = "4vjmo8p4y;kl346;mk435";

    private static int toByte(char c) {
        byte b = (byte) "0123456789ABCDEF".indexOf(c);
        return b;
    }

    public static byte[] hexStringToByteArray(String hex) {
        int len = (hex.length() / 2);
        byte[] result = new byte[len];
        char[] achar = hex.toCharArray();
        for (int i = 0; i < len; i++) {
            int pos = i * 2;
            result[i] = (byte) (toByte(achar[pos]) << 4 | toByte(achar[pos + 1]));
        }
        return result;
    }

    public static String decode(String in) {
        byte[] inData = hexStringToByteArray(in);
        byte[] outData = new byte[inData.length - 2];

        decode(inData, outData, 0, 0, inData.length);
        return new String(outData, 0, outData.length);
    }

    public static String encode(String in) {
        byte[] inData = in.getBytes();
        byte[] outData = new byte[inData.length + 2];

        encode(inData, outData, 0, 0, inData.length);
        return toHexString(outData);
    }

    public static String decodeAsString(byte[] inData) {
        if (needsLog) {
            return new String(inData, 0, inData.length);
        }
        byte[] outData = new byte[inData.length - 2];

        decode(inData, outData, 0, 0, inData.length);
        return new String(outData, 0, outData.length);
    }

    public static byte[] encodeAsByteArray(String in) {
        if (needsLog) {
            return in.getBytes();
        }
        byte[] inData = in.getBytes();
        byte[] outData = new byte[inData.length + 2];

        encode(inData, outData, 0, 0, inData.length);
        return outData;
    }

    static byte crc(byte[] data, int pos, int len) {
        int ret = data[pos];
        for (int i = 1; i < len; i++) {
            ret ^= data[pos + i];
        }
        return (byte) ret;
    }

    public static int encode(byte[] in, byte[] out, int inPos, int outPos, int len) {
        byte[] key = uuid.getBytes();

        for (int i = 0; i < len; i++) {
            out[outPos + i] = (byte) (key[i % key.length] ^ in[inPos + i]);
        }
        out[outPos + len] = crc(in, inPos, len);
        out[outPos + len + 1] = crc(out, outPos, len);
        return len + 2;
    }

    public static int decode(byte[] in, byte[] out, int inPos, int outPos, int len) {
        byte[] key = uuid.getBytes();

        byte crc1 = in[len - 2];
        byte crc2 = in[len - 1];
        byte calc_crc2 = crc(in, inPos, len - 2);
        if (calc_crc2 != crc2) {
            return -1;
        }

        for (int i = 0; i < len - 2; i++) {
            out[outPos + i] = (byte) (key[i % key.length] ^ in[inPos + i]);
        }

        byte calc_crc1 = crc(out, outPos, len - 2);
        if (calc_crc1 != crc1) {
            return -1;
        }

        return len - 2;
    }

    public static int toFile(byte[] src, String path) throws Exception {
        byte data[] = new byte[src.length + 2];
        HexUtil.encode(src, data, 0, 0, src.length);

        FileOutputStream fos = new FileOutputStream(path);
        fos.write(data);
        fos.close();

        return data.length;
    }

    public static int toFile(Object obj, String path) {
        try {
            ByteArrayOutputStream bos = new ByteArrayOutputStream();
            ObjectOutputStream oos = new ObjectOutputStream(bos);
            oos.writeObject(obj);
            return toFile(bos.toByteArray(), path);
        } catch (Exception e) {
            e.printStackTrace();
        }
        return -1;
    }

    public static byte[] fromFile(String path) throws Exception {
        try {
            byte[] buf = new byte[4096];
            int n = -1;

            FileInputStream fis = new FileInputStream(path);
            ByteArrayOutputStream bos = new ByteArrayOutputStream();
            while ((n = fis.read(buf, 0, 4096)) > 0) {
                bos.write(buf, 0, n);
            }
            fis.close();

            // decrypt
            byte src[] = bos.toByteArray();
            byte data[] = new byte[src.length - 2];
            HexUtil.decode(src, data, 0, 0, src.length);

            return data;
        } catch (Exception e) {
            e.printStackTrace();
        }
        return null;
    }

    public static Object objFromFile(String path) {
        try {
            byte data[] = fromFile(path);
            ByteArrayInputStream bis = new ByteArrayInputStream(data);
            ObjectInputStream ois = new ObjectInputStream(bis);
            Object obj = ois.readObject();
            return obj;
        } catch (Exception e) {
            e.printStackTrace();
        }
        return null;
    }

    public static int hexToInt(byte[] data, int offset) {
        int ret = 0;
        int digits = 4;
        for (int i = 0; i < digits; i++) {
            ret |= (data[offset + i] & 0xFF) << (8 * i);
        }
        return ret;
    }

    public static short hexToShort(byte[] data, int offset) {
        int ret = 0;
        int digits = 2;
        for (int i = 0; i < digits; i++) {
            ret |= (data[offset + i] & 0xFF) << (8 * i);
        }
        return (short) (ret & 0xFFFF);
    }

    public static byte[] intToHex(int value) {
        int digits = 4;
        byte[] ret = new byte[digits];
        for (int i = 0; i < digits; i++) {
            ret[i] = (byte) ((value >> (8 * i)) & 0xFF);
        }
        return ret;
    }

    public static byte[] longToHex(long value) {
        int digits = 8;
        byte[] ret = new byte[digits];
        for (int i = 0; i < digits; i++) {
            ret[i] = (byte) ((value >> (8 * i)) & 0xFF);
        }
        return ret;
    }

    public static byte[] shortToHex(short value) {
        int digits = 2;
        byte[] ret = new byte[digits];
        for (int i = 0; i < digits; i++) {
            ret[i] = (byte) ((value >> (8 * i)) & 0xFF);
        }
        return ret;
    }

    protected static char hexDigits[] = { '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'a', 'b', 'c', 'd', 'e',
            'f' };

    static String toHexString(byte[] bytes) {
        StringBuffer stringbuffer = new StringBuffer(2 * bytes.length);
        for (int l = 0; l < bytes.length; l++) {
            char c0 = hexDigits[(bytes[l] & 0xf0) >> 4];
            char c1 = hexDigits[bytes[l] & 0xf];
            stringbuffer.append(c0);
            stringbuffer.append(c1);
        }
        return stringbuffer.toString();
    }

}
