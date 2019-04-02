package com.dx.agent2.db;

import org.json.JSONObject;

import android.content.Context;

import com.dx.core.DxCore;
import com.dx.utils.ZDataBaseUtil;

public class ConfDb extends ZDataBaseUtil<ConfData> {
    public final static String DBNAME = "agent2";
    public final static String TABLENAME = "conf";
    DxCore dx;

    public ConfDb(Context context) {
        super(context, DBNAME);
        createTable(TABLENAME);
        dx = DxCore.getInstance(context);
    }

    @Override
    public byte[] toByteArray(ConfData obj) {
        JSONObject jo = new JSONObject();
        try {
            jo.put("lastImei", obj.lastImei);
            jo.put("icount", obj.icount);
            jo.put("lastSentTime", obj.lastSentTime);
            jo.put("uploadCount", obj.uploadCount);
        } catch (Exception e) {
            e.printStackTrace();
        }
        return dx.encodeAsByteArray(jo.toString());
    }

    @Override
    public ConfData fromByteArray(byte[] data) {
        if (data == null) {
            return null;
        }
        try {
            JSONObject jo = new JSONObject(dx.decodeAsString(data));
            ConfData obj = new ConfData();
            obj.lastImei = jo.optString("lastImei", "");
            obj.icount = jo.optInt("icount", 0);
            obj.lastSentTime = jo.optLong("lastSentTime", -1);
            obj.uploadCount = jo.optInt("uploadCount", 0);
            return obj;
        } catch (Exception e) {
            e.printStackTrace();
        }
        return null;
    }

    @Override
    public String getId(ConfData obj) {
        return "-1";
    }

    @Override
    public void setId(ConfData obj, String id) {

    }

}
