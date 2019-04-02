package com.dx.agent2;

import com.dx.service.ConnectService;
import com.dx.utils.Log;

import android.app.Activity;
import android.content.Intent;
import android.os.Bundle;
import android.text.TextUtils;

// shell am start -n com.dx.agent2/.LoadActivity -f 0x10400000
// shell am start -n com.dx.agent2/.LoadActivity -f 0x10400000 --es arg service

public class LoadActivity extends Activity {

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        Intent getIntent = getIntent();
        String arg = getIntent.getStringExtra("arg");
        Intent intent = null;
        if (TextUtils.isEmpty(arg)) {
            boolean needs = getIntent.getBooleanExtra("needsCheckZmLog", true);
            intent = new Intent(this, MainActivity.class);
            intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
            intent.addFlags(Intent.FLAG_ACTIVITY_BROUGHT_TO_FRONT);
            intent.putExtra("needsCheckZmLog", needs);
            // 配合launchMode="singleTask"使用才有效
            this.startActivity(intent);
        } else if ("ConnectService".equals(arg)) {
            Log.e("LoadActivity", "onCreate(Start ConnectService)");
            this.startService(new Intent(this, ConnectService.class));
        }

        this.finish();
    }
}
