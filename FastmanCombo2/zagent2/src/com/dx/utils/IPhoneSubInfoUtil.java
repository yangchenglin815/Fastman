package com.dx.utils;

import java.lang.reflect.Method;
import java.util.ArrayList;
import java.util.Collections;
import java.util.Comparator;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.Map.Entry;
import java.util.Set;

import android.content.Context;
import android.os.IBinder;
import android.telephony.TelephonyManager;
import android.text.TextUtils;

public class IPhoneSubInfoUtil {
    public static final String TAG = IPhoneSubInfoUtil.class.getSimpleName();

    public static List<Object> getIphonesubinfoList() throws Exception {
        List<Object> ret = new ArrayList<Object>();

        Class<?> cls_sm = Class.forName("android.os.ServiceManager");
        Method m_listservice = cls_sm.getMethod("listServices");
        Method m_getservice = cls_sm.getMethod("getService", String.class);

        Class<?> cls_stub = Class.forName("com.android.internal.telephony.IPhoneSubInfo$Stub");
        Method m_asintf = cls_stub.getMethod("asInterface", IBinder.class);

        Log.i("", "find iphonesubinfo using listServices method");
        String[] services = (String[]) m_listservice.invoke(null);
        for (String service : services) {
            if (service != null && service.contains("iphonesubinfo")) {
                Log.i("", "found service " + service);
                IBinder b = (IBinder) m_getservice.invoke(null, service);
                Object obj = m_asintf.invoke(null, b);
                ret.add(obj);
            }
        }

        // try old method for inferior phones
        if (ret.size() < 1) {
            Log.i("", "find iphonesubinfo using blackbox method");
            String[] list = { "iphonesubinfo", "iphonesubinfo1", "iphonesubinfo2", "iphonesubinfo3", "iphonesubinfo4",
                    "iphonesubinfo5" };
            for (String service : list) {
                IBinder b = (IBinder) m_getservice.invoke(null, service);
                if (b != null) {
                    Log.i("", "found service " + service);
                    Object obj = m_asintf.invoke(null, b);
                    ret.add(obj);
                }
            }
        }

        return ret;
    }

    public static Map<String, String> getImeiMap(Context context) {
        Map<String, String> map = new HashMap<String, String>();

        // let's get from common methods first
        TelephonyManager tm = (TelephonyManager) context.getSystemService(Context.TELEPHONY_SERVICE);
        String imei = tm.getDeviceId();
        String imsi = tm.getSubscriberId();
        if (imei != null && imei.length() > 0) {
            if (TextUtils.isEmpty(map.get(imei))) {
                map.put(imei, imsi);
                Log.i(TAG, "sdk0： IMEI " + imei + " -> IMSI " + imsi);
            } else {
                Log.i(TAG, "sdk1： IMEI " + imei + " -> IMSI " + imsi);
            }
        }

        do {
            Class<?> cls_iphonesubinfo = null;
            Method m_getimei = null;
            Method m_getimsi = null;
            Method m_getdualimei = null;
            Method m_getdualimsi = null;

            try {
                cls_iphonesubinfo = Class.forName("com.android.internal.telephony.IPhoneSubInfo");
                m_getimei = cls_iphonesubinfo.getMethod("getDeviceId");
                m_getimsi = cls_iphonesubinfo.getMethod("getSubscriberId");
                m_getdualimei = cls_iphonesubinfo.getMethod("getDualDeviceId", int.class);
                m_getdualimsi = cls_iphonesubinfo.getMethod("getDualSubscriberId", int.class);
            } catch (Exception e) {
            }

            try {
                List<Object> objList = getIphonesubinfoList();
                for (Object obj : objList) {
                    imei = (String) m_getimei.invoke(obj);
                    imsi = (String) m_getimsi.invoke(obj);
                    if (imei != null && imei.length() > 0) {
                        if (TextUtils.isEmpty(map.get(imei))) {
                            map.put(imei, imsi);
                            Log.i(TAG, "IPhoneSubInfo.getxx method0 " + "IMEI " + imei + " -> IMSI " + imsi);
                        } else {
                            Log.i(TAG, "IPhoneSubInfo.getxx method1 " + "IMEI " + imei + " -> IMSI " + imsi);
                        }
                    }

                    if (m_getdualimei != null && m_getdualimsi != null) {
                        for (int i = 0; i < 5; i++) {
                            imei = (String) m_getdualimei.invoke(obj, i);
                            imsi = (String) m_getdualimsi.invoke(obj, i);
                            if (imei != null && imei.length() > 0) {
                                if (TextUtils.isEmpty(map.get(imei))) {
                                    map.put(imei, imsi);
                                    Log.i(TAG, "IPhoneSubInfo.getDualxx method0 " + i + " IMEI " + imei + " -> IMSI "
                                            + imsi);
                                } else {
                                    Log.i(TAG, "IPhoneSubInfo.getDualxx method1 " + i + " IMEI " + imei + " -> IMSI "
                                            + imsi);
                                }
                            }
                        }
                    }
                }
            } catch (Exception e) {
                e.printStackTrace();
            }

            try {
                Class<?> cls_multiSimTM = Class.forName("android.telephony.MSimTelephonyManager");
                Method m_multiSimGetDefault = cls_multiSimTM.getMethod("getDefault");
                Method m_multiSimGetPhoneCount = cls_multiSimTM.getMethod("getPhoneCount");
                Method m_multiSimGetDeviceId = cls_multiSimTM.getMethod("getDeviceId", int.class);
                Method m_multiSimGetSubscriberId = cls_multiSimTM.getMethod("getSubscriberId", int.class);

                Object multiSimTM = m_multiSimGetDefault.invoke(null);
                int phoneCount = (Integer) m_multiSimGetPhoneCount.invoke(multiSimTM);
                for (int i = 0; i < phoneCount; i++) {
                    imei = (String) m_multiSimGetDeviceId.invoke(multiSimTM, i);
                    imsi = (String) m_multiSimGetSubscriberId.invoke(multiSimTM, i);
                    if (imei != null && imei.length() > 0) {
                        if (TextUtils.isEmpty(map.get(imei))) {
                            map.put(imei, imsi);
                            Log.i(TAG, "MSimTelephonyManager0 " + i + " IMEI " + imei + " -> IMSI " + imsi);
                        } else {
                            Log.i(TAG, "MSimTelephonyManager1 " + i + " IMEI " + imei + " -> IMSI " + imsi);
                        }
                    }
                }
            } catch (Exception e) {
                // e.printStackTrace();
            }
        } while (false);

        return map;
    }

    public static List<String> getImeiList(Context context) {
        List<String> ret = new ArrayList<String>();
        Map<String, String> map = getImeiMap(context);
        Set<Entry<String, String>> sets = map.entrySet();
        for (Entry<String, String> entry : sets) {
            ret.add(entry.getKey());
        }
        return ret;
    }

    public static List<String> getImsiList(Context context) {
        List<String> ret = new ArrayList<String>();
        Map<String, String> map = getImeiMap(context);
        Set<Entry<String, String>> sets = map.entrySet();
        for (Entry<String, String> entry : sets) {
            ret.add(entry.getValue());
        }
        return ret;
    }

    public static String getSmallestImei(Context context) {
        String ret = null;
        Map<String, String> map = getImeiMap(context);
        Set<Entry<String, String>> sets = map.entrySet();
        for (Entry<String, String> entry : sets) {
            if (ret == null || entry.getKey().compareToIgnoreCase(ret) < 0) {
                ret = entry.getKey();
            }
        }
        if (ret == null) {
            ret = "null";
        }
        return ret;
    }

    public static String getAllImei(Context context, boolean isSort) {
        StringBuilder sb = new StringBuilder();
        List<String> list = new ArrayList<String>();
        Map<String, String> map = getImeiMap(context);
        Set<Entry<String, String>> sets = map.entrySet();
        for (Entry<String, String> entry : sets) {
            if (entry.getKey() == null) {
                list.add("null");
            } else {
                list.add(entry.getKey());
            }
        }
        if (isSort) {
            Collections.sort(list, new StringComparator());
        }
        boolean isfirst = true;
        for (String imei : list) {
            if (!isfirst) {
                sb.append(",");
            }
            sb.append(imei);
            isfirst = false;
        }
        return sb.toString();
    }

    public static String getAllImsi(Context context, boolean isSort) {
        StringBuilder sb = new StringBuilder();
        List<String> list = new ArrayList<String>();
        Map<String, String> map = getImeiMap(context);
        Set<Entry<String, String>> sets = map.entrySet();
        for (Entry<String, String> entry : sets) {
            if (entry.getValue() == null) {
                list.add("null");
            } else {
                list.add(entry.getValue());
            }
        }

        if (isSort) {
            Collections.sort(list, new StringComparator());
        }
        boolean isfirst = true;
        for (String imsi : list) {
            if (!isfirst) {
                sb.append(",");
            }
            sb.append(imsi);
            isfirst = false;
        }
        return sb.toString();
    }

    public static class StringComparator implements Comparator<String> {
        public int compare(String arg0, String arg1) {
            return arg0.compareToIgnoreCase(arg1);
        }
    }

    public static String getImeiString(Context context) {
        return getImeiList(context).toString().replace(" ", "");
    }

    public static String getImsiString(Context context) {
        return getImsiList(context).toString().replace(" ", "");
    }

    // 厂商|机型|版本名称
    public static String getPattern() {
        String model = android.os.Build.MANUFACTURER + "|" + android.os.Build.MODEL + "|"
                + android.os.Build.VERSION.RELEASE;
        return model.replace(" ", "");
    }

}
