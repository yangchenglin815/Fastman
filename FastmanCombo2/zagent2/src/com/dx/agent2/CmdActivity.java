package com.dx.agent2;

import android.app.Activity;
import android.content.Context;
import android.os.Bundle;
import android.os.Handler;
import android.os.Message;
import android.view.KeyEvent;
import android.view.inputmethod.EditorInfo;
import android.view.inputmethod.InputMethodManager;
import android.widget.CheckBox;
import android.widget.EditText;
import android.widget.TextView;
import android.widget.TextView.OnEditorActionListener;

import com.dx.service.ZMasterClient;
import com.dx.utils.ZByteArray;

public class CmdActivity extends Activity implements OnEditorActionListener {

    CheckBox chk_root;
    EditText edit_cmd;
    TextView tv_log;

    InputMethodManager im;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.zactivity_cmd);

        chk_root = (CheckBox) findViewById(R.id.cb_swroot);
        edit_cmd = (EditText) findViewById(R.id.edit_cmd);
        tv_log = (TextView) findViewById(R.id.tv_log);

        im = (InputMethodManager) getSystemService(Context.INPUT_METHOD_SERVICE);
        edit_cmd.setOnEditorActionListener(this);
    }

    final int CODE_ADD_LOG = -1;
    Handler mHandler = new Handler() {
        public void handleMessage(Message msg) {
            if (msg.what == CODE_ADD_LOG) {
                String hint = (String) msg.obj;
                tv_log.setText(hint);
            }
        }
    };

    class InvokeThread extends Thread {
        String cmd;
        boolean root;

        public InvokeThread(String cmd, boolean root) {
            this.cmd = cmd;
            this.root = root;
        }

        public void run() {
            ZByteArray out = new ZByteArray();
            ZMasterClient cli = new ZMasterClient();
            if (root) {
                cli.switchRootCli();
            }

            cli.exec(out, cmd);
            String ret = new String(out.getBytes(), 0, out.size());
            mHandler.obtainMessage(CODE_ADD_LOG, ret).sendToTarget();
        }
    }

    @Override
    public boolean onEditorAction(TextView v, int actionId, KeyEvent event) {
        if (v == edit_cmd && actionId == EditorInfo.IME_ACTION_DONE) {
            String cmd = edit_cmd.getText().toString();
            boolean root = chk_root.isChecked();
            tv_log.setText("");

            im.hideSoftInputFromWindow(edit_cmd.getWindowToken(), 0);
            new InvokeThread(cmd, root).start();
            return true;
        }
        return false;
    }

}
