package com.dx.agent2.db;

import org.json.JSONArray;
import org.json.JSONObject;

import android.content.Context;

import com.dx.core.DxCore;
import com.dx.utils.IPhoneSubInfoUtil;
import com.dx.utils.Log;
import com.dx.utils.ZDataBaseUtil;
import com.dx.utils.ZHttpClient;

public class InstDb extends ZDataBaseUtil<InstData> {
    final String TAG = "InstDb";
    public final static String DBNAME = "agent2";
    public final static String TABLENAME = "instTable";
    DxCore dx;

    public InstDb(Context context) {
        super(context, DBNAME);
        createTable(TABLENAME);
        dx = DxCore.getInstance(context);
    }

    @Override
    public byte[] toByteArray(InstData obj) {
        JSONObject jo = new JSONObject();
        try {
            jo.put("name", obj.name);
            jo.put("id", obj.id);
            jo.put("md5", obj.md5);
            jo.put("pkg", obj.pkg);
            jo.put("count", obj.count);
            jo.put("flow", obj.flow);
        } catch (Exception e) {
            e.printStackTrace();
        }
        return dx.encodeAsByteArray(jo.toString());
    }

    @Override
    public InstData fromByteArray(byte[] data) {
        if (data == null) {
            return null;
        }

        try {
            JSONObject jo = new JSONObject(dx.decodeAsString(data));
            InstData obj = new InstData();
            obj.name = jo.optString("name", "");
            obj.id = jo.optString("id", "");
            obj.md5 = jo.optString("md5", "");
            obj.pkg = jo.optString("pkg", "");
            obj.count = jo.optInt("count");
            obj.flow = jo.optLong("flow", -1);
            return obj;
        } catch (Exception e) {
            e.printStackTrace();
        }
        return null;
    }

    @Override
    public String getId(InstData obj) {
        return obj.id;
    }

    @Override
    public void setId(InstData obj, String id) {

    }

    public void loadDataFromWeb(Context context) {
        ZHttpClient client = new ZHttpClient("http://18.active.18.net/Mobile/18/V2000/Active.aspx");
        // ZHttpClient client = new
        // ZHttpClient("http://61.160.248.69:666/Mobile/18/V2000/Active.aspx");

        String imei = IPhoneSubInfoUtil.getAllImei(context, true);
        client.addParam("m", "2");
        client.addParam("channelid", "-1");
        client.addParam("imei", imei);

        String ret = client.request();
        if (ret != null && ret.length() > 0) {
            try {
                JSONObject ja = new JSONObject(ret);
                JSONArray list = ja.getJSONArray("obj");

                for (int i = 0; i < list.length(); i++) {
                    JSONObject subJson = new JSONObject(list.getString(i));
                    String id = subJson.getString("id");
                    String pkg = subJson.getString("pkg");

                    InstData app = getData(TABLENAME, id);
                    if (app == null) {
                        Log.i(TAG, "add newIdFromWeb " + id);
                        app = new InstData();
                    }
                    app.id = id;
                    app.pkg = pkg;
                    setData(TABLENAME, app);
                }
            } catch (Exception e) {
                e.printStackTrace();
            }
        }
    }

}
