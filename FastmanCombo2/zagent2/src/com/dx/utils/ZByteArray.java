package com.dx.utils;

import java.nio.ByteBuffer;
import java.nio.ByteOrder;

import android.text.TextUtils;

public class ZByteArray {
    byte[] _data = null;

    public ZByteArray() {
        _data = new byte[0];
    }

    public int size() {
        return _data == null ? 0 : _data.length;
    }

    public static String toHex(byte[] _data) {
        StringBuilder sb = new StringBuilder();
        for (byte b : _data) {
            sb.append(String.format("%02X", b));
        }
        return sb.toString();
    }

    public String toHex() {
        return toHex(_data);
    }

    public static String toString(byte[] _data) {
        StringBuilder sb = new StringBuilder();
        for (int i = 0; i < _data.length; i += 32) {
            for (int j = i; j < i + 32; j++) {
                if (j < _data.length) {
                    sb.append(String.format("%02X", _data[j]));
                } else {
                    sb.append("  ");
                }
            }
            sb.append(" -- ");

            for (int j = i; j < i + 32; j++) {
                if (j < _data.length) {
                    byte b = _data[j];
                    if (b > 31 && b < 127) {
                        sb.append((char) b);
                    } else {
                        sb.append('.');
                    }
                } else {
                    sb.append(' ');
                }
            }
            sb.append('\n');
        }
        return sb.toString();
    }

    public String toString() {
        return toString(_data);
    }

    public void resize(int newsize) {
        byte[] _newdata = new byte[newsize];
        System.arraycopy(_data, 0, _newdata, 0, _data.length > newsize ? newsize : _data.length);
        _data = _newdata;
    }

    public byte[] remove(int from, int length) {
        int len = length;
        int left = 0;
        if (from + length >= _data.length) {
            len = _data.length - from;
        } else {
            left = _data.length - (from + len);
        }
        byte[] d = new byte[len];
        byte[] r = new byte[left];
        System.arraycopy(_data, from, d, 0, len);
        System.arraycopy(_data, from + len, r, 0, left);
        resize(from);
        putBytes(r);
        return d;
    }

    public byte[] getBytes() {
        byte d[] = new byte[_data.length];
        System.arraycopy(_data, 0, d, 0, _data.length);
        return d;
    }

    public byte[] getBytes(int from, int len) {
        byte d[] = new byte[len];
        System.arraycopy(_data, from, d, 0, len);
        return d;
    }

    public void insert(int from, byte[] d) {
        if (from > _data.length - 1) {
            resize(from + d.length);
            for (int i = _data.length - 1; i < from; i++) {
                _data[i] = 0;
            }
            System.arraycopy(d, 0, _data, from, d.length);
        } else {
            int left = _data.length - from;
            byte[] r = new byte[left];
            System.arraycopy(_data, from, r, 0, left);
            resize(from + d.length);
            System.arraycopy(d, 0, _data, from, d.length);
            putBytes(r);
        }
    }

    public void clear() {
        _posGet = 0;
        resize(0);
    }

    public ZByteArray mid(int from, int length) {
        int len = length;
        if (length < 0) {
            len = _data.length - from;
        }
        byte[] d = new byte[len];
        System.arraycopy(_data, from, d, 0, len);
        ZByteArray ret = new ZByteArray();
        ret.putBytes(d);
        return ret;
    }

    public void putByte(byte b) {
        resize(_data.length + 1);
        _data[_data.length - 1] = b;
    }

    public void putBytes(byte[] b) {
        resize(_data.length + b.length);
        System.arraycopy(b, 0, _data, _data.length - b.length, b.length);
    }

    public void putBytes(byte[] b, int from, int len) {
        resize(_data.length + len);
        System.arraycopy(b, from, _data, _data.length - len, len);
    }

    public void putShort(short s) {
        resize(_data.length + 2);
        ByteBuffer b = ByteBuffer.allocate(2).order(ByteOrder.LITTLE_ENDIAN);
        b.putShort(s);
        b.rewind();
        b.get(_data, _data.length - 2, 2);
    }

    public void putInt(int i) {
        resize(_data.length + 4);
        ByteBuffer b = ByteBuffer.allocate(4).order(ByteOrder.LITTLE_ENDIAN);
        b.putInt(i);
        b.rewind();
        b.get(_data, _data.length - 4, 4);
    }

    public void putInt64(long l) {
        resize(_data.length + 8);
        ByteBuffer b = ByteBuffer.allocate(8).order(ByteOrder.LITTLE_ENDIAN);
        b.putLong(l);
        b.rewind();
        b.get(_data, _data.length - 8, 8);
    }

    public void putUtf8(String str, boolean addHead) {
        if (TextUtils.isEmpty(str)) {
            if (addHead) {
                putInt(0);
            }
            return;
        }

        try {
            byte[] d = str.getBytes("UTF-8");
            if (addHead) {
                putInt(d.length);
            }
            putBytes(d);
        } catch (Exception e) {
            e.printStackTrace();
        }
    }

    private int _posGet = 0;

    public byte getByte(int i) {
        return _data[i];
    }

    public byte getByte() {
        byte ret = _data[_posGet];
        _posGet += 1;
        return ret;
    }

    public short getShort(int i) {
        ByteBuffer b = ByteBuffer.allocate(2).order(ByteOrder.LITTLE_ENDIAN);
        b.put(_data, i, 2);
        b.rewind();
        return b.getShort();
    }

    public short getShort() {
        ByteBuffer b = ByteBuffer.allocate(2).order(ByteOrder.LITTLE_ENDIAN);
        b.put(_data, _posGet, 2);
        b.rewind();
        short ret = b.getShort();
        _posGet += 2;
        return ret;
    }

    public int getInt(int i) {
        ByteBuffer b = ByteBuffer.allocate(4).order(ByteOrder.LITTLE_ENDIAN);
        b.put(_data, i, 4);
        b.rewind();
        return b.getInt();
    }

    public int getInt() {
        ByteBuffer b = ByteBuffer.allocate(4).order(ByteOrder.LITTLE_ENDIAN);
        b.put(_data, _posGet, 4);
        b.rewind();
        int ret = b.getInt();
        _posGet += 4;
        return ret;
    }

    public long getInt64(int i) {
        ByteBuffer b = ByteBuffer.allocate(8).order(ByteOrder.LITTLE_ENDIAN);
        b.put(_data, i, 8);
        b.rewind();
        return b.getLong();
    }

    public long getInt64() {
        ByteBuffer b = ByteBuffer.allocate(8).order(ByteOrder.LITTLE_ENDIAN);
        b.put(_data, _posGet, 8);
        b.rewind();
        long ret = b.getLong();
        _posGet += 8;
        return ret;
    }

    public String getUtf8(int i) {
        try {
            int len = getInt(i);
            return new String(_data, i + 4, len, "UTF-8");
        } catch (Exception e) {
            e.printStackTrace();
        }
        return "";
    }

    public String getUtf8() {
        try {
            int len = getInt();
            String ret = new String(_data, _posGet, len, "UTF-8");
            _posGet += len;
            return ret;
        } catch (Exception e) {
            e.printStackTrace();
        }
        return "";
    }

    public String getUtf8(int i, int len) {
        try {
            return new String(_data, i, len, "UTF-8");
        } catch (Exception e) {
            e.printStackTrace();
        }
        return "";
    }

    public static short checksum(byte[] data, int from, int length) {
        int ret = 0;
        for (int i = from; i < from + length; i++) {
            byte b = data[i];
            int j = ((ret >> 8) ^ b) & 0xff;
            ret = (ret << 8) ^ crc16table[j];
        }
        return (short) ret;
    }

    public short checksum(int from, int length) {
        return checksum(_data, from, length);
    }

    public int indexOf(byte[] d) {
        return byteArraySearch(_data, d);
    }

    public static int byteArraySearch(byte[] _data, byte[] key) {
        for (int i = 0; i < _data.length; i++) {
            if (byteArrayStartsWith(_data, i, key)) {
                return i;
            }
        }
        return -1;
    }

    public static boolean byteArrayStartsWith(byte[] _data, int from, byte[] key) {
        if (_data.length - from < key.length) {
            return false;
        }

        for (int i = 0; i < key.length; i++) {
            if (_data[from + i] != key[i]) {
                return false;
            }
        }
        return true;
    }

    public static byte[] shortToByteArray(short s) {
        byte[] data = new byte[2];
        ByteBuffer b = ByteBuffer.allocate(2).order(ByteOrder.LITTLE_ENDIAN);
        b.putShort(s);
        b.rewind();
        b.get(data, 0, 2);
        return data;
    }

    public static byte[] intToByteArray(int i) {
        byte[] data = new byte[4];
        ByteBuffer b = ByteBuffer.allocate(4).order(ByteOrder.LITTLE_ENDIAN);
        b.putInt(i);
        b.rewind();
        b.get(data, 0, 4);
        return data;
    }

    public static byte[] int64ToByteArray(long l) {
        byte[] data = new byte[8];
        ByteBuffer b = ByteBuffer.allocate(8).order(ByteOrder.LITTLE_ENDIAN);
        b.putLong(l);
        b.rewind();
        b.get(data, 0, 8);
        return data;
    }

    private static final int crc16table[] = { 0x0000, 0xC0C1, 0xC181, 0x0140, 0xC301, 0x03C0, 0x0280, 0xC241, 0xC601,
            0x06C0, 0x0780, 0xC741, 0x0500, 0xC5C1, 0xC481, 0x0440, 0xCC01, 0x0CC0, 0x0D80, 0xCD41, 0x0F00, 0xCFC1,
            0xCE81, 0x0E40, 0x0A00, 0xCAC1, 0xCB81, 0x0B40, 0xC901, 0x09C0, 0x0880, 0xC841, 0xD801, 0x18C0, 0x1980,
            0xD941, 0x1B00, 0xDBC1, 0xDA81, 0x1A40, 0x1E00, 0xDEC1, 0xDF81, 0x1F40, 0xDD01, 0x1DC0, 0x1C80, 0xDC41,
            0x1400, 0xD4C1, 0xD581, 0x1540, 0xD701, 0x17C0, 0x1680, 0xD641, 0xD201, 0x12C0, 0x1380, 0xD341, 0x1100,
            0xD1C1, 0xD081, 0x1040, 0xF001, 0x30C0, 0x3180, 0xF141, 0x3300, 0xF3C1, 0xF281, 0x3240, 0x3600, 0xF6C1,
            0xF781, 0x3740, 0xF501, 0x35C0, 0x3480, 0xF441, 0x3C00, 0xFCC1, 0xFD81, 0x3D40, 0xFF01, 0x3FC0, 0x3E80,
            0xFE41, 0xFA01, 0x3AC0, 0x3B80, 0xFB41, 0x3900, 0xF9C1, 0xF881, 0x3840, 0x2800, 0xE8C1, 0xE981, 0x2940,
            0xEB01, 0x2BC0, 0x2A80, 0xEA41, 0xEE01, 0x2EC0, 0x2F80, 0xEF41, 0x2D00, 0xEDC1, 0xEC81, 0x2C40, 0xE401,
            0x24C0, 0x2580, 0xE541, 0x2700, 0xE7C1, 0xE681, 0x2640, 0x2200, 0xE2C1, 0xE381, 0x2340, 0xE101, 0x21C0,
            0x2080, 0xE041, 0xA001, 0x60C0, 0x6180, 0xA141, 0x6300, 0xA3C1, 0xA281, 0x6240, 0x6600, 0xA6C1, 0xA781,
            0x6740, 0xA501, 0x65C0, 0x6480, 0xA441, 0x6C00, 0xACC1, 0xAD81, 0x6D40, 0xAF01, 0x6FC0, 0x6E80, 0xAE41,
            0xAA01, 0x6AC0, 0x6B80, 0xAB41, 0x6900, 0xA9C1, 0xA881, 0x6840, 0x7800, 0xB8C1, 0xB981, 0x7940, 0xBB01,
            0x7BC0, 0x7A80, 0xBA41, 0xBE01, 0x7EC0, 0x7F80, 0xBF41, 0x7D00, 0xBDC1, 0xBC81, 0x7C40, 0xB401, 0x74C0,
            0x7580, 0xB541, 0x7700, 0xB7C1, 0xB681, 0x7640, 0x7200, 0xB2C1, 0xB381, 0x7340, 0xB101, 0x71C0, 0x7080,
            0xB041, 0x5000, 0x90C1, 0x9181, 0x5140, 0x9301, 0x53C0, 0x5280, 0x9241, 0x9601, 0x56C0, 0x5780, 0x9741,
            0x5500, 0x95C1, 0x9481, 0x5440, 0x9C01, 0x5CC0, 0x5D80, 0x9D41, 0x5F00, 0x9FC1, 0x9E81, 0x5E40, 0x5A00,
            0x9AC1, 0x9B81, 0x5B40, 0x9901, 0x59C0, 0x5880, 0x9841, 0x8801, 0x48C0, 0x4980, 0x8941, 0x4B00, 0x8BC1,
            0x8A81, 0x4A40, 0x4E00, 0x8EC1, 0x8F81, 0x4F40, 0x8D01, 0x4DC0, 0x4C80, 0x8C41, 0x4400, 0x84C1, 0x8581,
            0x4540, 0x8701, 0x47C0, 0x4680, 0x8641, 0x8201, 0x42C0, 0x4380, 0x8341, 0x4100, 0x81C1, 0x8081, 0x4040 };

}
