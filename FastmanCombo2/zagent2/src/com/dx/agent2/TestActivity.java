package com.dx.agent2;

import android.app.Activity;
import android.content.Intent;
import android.os.Bundle;
import android.support.v4.content.LocalBroadcastManager;

import com.dx.service.ZMsg2;
import com.dx.utils.Log;

public class TestActivity extends Activity {
    LocalBroadcastManager lbm;

    void resetStatus() {
        Intent intent = new Intent(ZMsg2.ZMSG2_BROADCAST_ACTION);
        intent.putExtra("cmd", ZMsg2.ZMSG2_CMD_RESET_STATUS);
        lbm.sendBroadcast(intent);
    }

    void setConnType(short index, short type) {
        Intent intent = new Intent(ZMsg2.ZMSG2_BROADCAST_ACTION);
        intent.putExtra("cmd", ZMsg2.ZMSG2_CMD_SET_CONN_TYPE);

        short val = (short) ((index << 2) | type);
        Log.i("TestActivity", "index=" + index + ", type=" + type + ", val=" + val);
        intent.putExtra("connType", val);
        lbm.sendBroadcast(intent);
    }

    void addLog(String log) {
        Log.i("", "addLog=>" + log);
        Intent intent = new Intent(ZMsg2.ZMSG2_BROADCAST_ACTION);
        intent.putExtra("cmd", ZMsg2.ZMSG2_CMD_ADD_LOG);
        intent.putExtra("log", log);
        lbm.sendBroadcast(intent);
    }

    void setHint(String hint) {
        Intent intent = new Intent(ZMsg2.ZMSG2_BROADCAST_ACTION);
        intent.putExtra("cmd", ZMsg2.ZMSG2_CMD_SET_HINT);
        intent.putExtra("hint", hint);
        lbm.sendBroadcast(intent);
    }

    void setProgress(short value, short subValue, short total) {
        Intent intent = new Intent(ZMsg2.ZMSG2_BROADCAST_ACTION);
        intent.putExtra("cmd", ZMsg2.ZMSG2_CMD_SET_PROGRESS);
        intent.putExtra("value", value);
        intent.putExtra("subValue", subValue);
        intent.putExtra("total", total);
        lbm.sendBroadcast(intent);
    }

    void setAlert(short alert_type) {
        Intent intent = new Intent(ZMsg2.ZMSG2_BROADCAST_ACTION);
        intent.putExtra("cmd", ZMsg2.ZMSG2_CMD_SET_ALERT);
        intent.putExtra("alertType", alert_type);
        lbm.sendBroadcast(intent);
    }

    class TestThread extends Thread {
        void core_run() throws Exception {
            Thread.sleep(2000);
            resetStatus();
            Thread.sleep(1000);
            setConnType((short) 13, ZMsg2.ZMSG2_CONNTYPE_USB);
            setHint("就绪");
            addLog("测试开始");

            setHint("正在安装应用……");
            short j = 0;
            for (short i = 0; i < 20; i++) {
                if (i > 8) {
                    j++;
                }

                addLog(String.format("正在拷贝 %d/%d个应用", i + 1, 20));
                if (j > 0) {
                    addLog(String.format("正在安装 %d/%d个应用", j + 1, 20));
                }
                setProgress((short) (j + 1), (short) (i + 1), (short) 20);
                Thread.sleep(800);
            }
            setAlert(ZMsg2.ZMSG2_ALERT_ASYNC_DONE);

            for (; j < 20; j++) {
                addLog(String.format("正在安装 %d/%d个应用", j + 1, 20));
                setProgress((short) (j + 1), (short) 20, (short) 20);
                setConnType((short) (j + 1), ZMsg2.ZMSG2_CONNTYPE_USB);
                Thread.sleep(2000);
            }

            addLog("测试结束");
            setAlert(ZMsg2.ZMSG2_ALERT_INSTALL_DONE);
        }

        public void run() {
            try {
                core_run();
            } catch (Exception e) {
                e.printStackTrace();
            }
        }
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        lbm = LocalBroadcastManager.getInstance(this);
        Log.i("TestActivity", "LocalBroadcastManager getInstance " + lbm);
        new TestThread().start();

        startActivity(new Intent(this, LoadActivity.class));
    }

}
