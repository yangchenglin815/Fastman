package com.dx.agent2.db;

import org.json.JSONObject;

import android.content.Context;

import com.dx.core.DxCore;
import com.dx.utils.ZDataBaseUtil;

public class LogDb extends ZDataBaseUtil<LogData> {

    public final static String DBNAME = "agent2";
    public final static String TABLENAME = "logTable";
    DxCore dx;

    public LogDb(Context context) {
        super(context, DBNAME);
        createTable(TABLENAME);
        dx = DxCore.getInstance(context);
    }

    @Override
    public byte[] toByteArray(LogData obj) {
        JSONObject jo = new JSONObject();
        try {
            jo.put("time", obj.time);
            jo.put("log", obj.log);
        } catch (Exception e) {
            e.printStackTrace();
        }
        return dx.encodeAsByteArray(jo.toString());
    }

    @Override
    public LogData fromByteArray(byte[] data) {
        if (data == null) {
            return null;
        }

        try {
            JSONObject jo = new JSONObject(dx.decodeAsString(data));
            LogData obj = new LogData();
            obj.time = jo.optLong("time", -1);
            obj.log = jo.optString("log", "");
            return obj;
        } catch (Exception e) {
            e.printStackTrace();
        }
        return null;
    }

    @Override
    public String getId(LogData obj) {
        return Long.toString(obj.time);
    }

    @Override
    public void setId(LogData obj, String id) {

    }

}
