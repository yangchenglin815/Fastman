package com.dx.service;

import java.io.ByteArrayOutputStream;
import java.io.InputStream;
import java.io.OutputStream;
import java.net.Socket;

import com.dx.utils.Log;
import com.dx.utils.ZByteArray;

class ZMasterSocket {
    private final String TAG = "ZMasterSocket";

    private Socket _socket = null;
    private InputStream _ins = null;
    private OutputStream _ous = null;
    private int _port = 19001;
    private int _timeout = 10000;

    private byte[] readx(int len) throws Throwable {
        ByteArrayOutputStream bos = new ByteArrayOutputStream();
        int left = len;
        byte[] buf = new byte[4096];
        int n;
        while (left > 0) {
            n = _ins.read(buf, 0, left > 4096 ? 4096 : left);
            if (n > 0) {
                bos.write(buf, 0, n);
                left -= n;
            } else {
                break;
            }
        }
        return bos.toByteArray();
    }

    public boolean connectToZMaster() {
        if (_socket != null && _socket.isConnected()) {
            return true;
        }

        try {
            _socket = new Socket("127.0.0.1", _port);
            _socket.setSoTimeout(_timeout);

            _ins = _socket.getInputStream();
            _ous = _socket.getOutputStream();
        } catch (Throwable e) {
            e.printStackTrace();
            return false;
        }
        return true;
    }

    public void disconnect() {
        try {
            if (_ins != null) {
                _ins.close();
            }

            if (_ous != null) {
                _ous.close();
            }

            if (_socket != null) {
                _socket.close();
            }
        } catch (Throwable t) {
            t.printStackTrace();
        }

        _ins = null;
        _ous = null;
        _socket = null;
    }

    public boolean sendAndRecvMsg(ZMsg2 msg) {
        if (!connectToZMaster()) {
            return false;
        }

        try {
            byte[] pkt = msg.getPacket().getBytes();
            _ous.write(pkt);

            ZByteArray packet = new ZByteArray();

            pkt = readx(8);
            if (pkt.length != 8) {
                Log.i(TAG, "read head error!");
            }
            packet.putBytes(pkt);

            do {
                int magic = packet.getInt(0);
                int left = packet.getInt(4);
                if (magic != ZMsg2.ZMSG2_MAGIC) {
                    Log.i(TAG, "invalid magic!\n");
                    break;
                }

                left += 2;
                pkt = readx(left);
                if (pkt.length != left) {
                    Log.i(TAG, "invalid read left\n");
                    break;
                }
                packet.putBytes(pkt);
                return msg.parse(packet);
            } while (false);
        } catch (Throwable e) {
            e.printStackTrace();
        }
        return false;
    }
}

public class ZMasterClient {
    ZMasterSocket _socket;

    public ZMasterClient() {
        _socket = new ZMasterSocket();
    }

    public boolean switchRootCli() {
        ZMsg2 msg = new ZMsg2();
        msg.cmd = ZMsg2.ZMSG2_SWITCH_ROOT;
        return _socket.sendAndRecvMsg(msg);
    }

    public int exec(ZByteArray out, String cmd) {
        ZMsg2 msg = new ZMsg2();
        msg.cmd = ZMsg2.ZMSG2_CMD_EXEC;
        msg.data.putInt(3);
        msg.data.putUtf8("/system/bin/sh", true);
        msg.data.putUtf8("-c", true);
        msg.data.putUtf8(cmd, true);

        if (!_socket.sendAndRecvMsg(msg)) {
            return -1;
        }

        int ret = msg.data.getInt(0);
        msg.data.remove(0, 8);
        out.putBytes(msg.data.getBytes());
        return ret;
    }

    public boolean remove(String path) {
        ZMsg2 msg = new ZMsg2();
        msg.cmd = ZMsg2.ZMSG2_CMD_RM;
        msg.data.putUtf8(path, true);
        return _socket.sendAndRecvMsg(msg);
    }
}
