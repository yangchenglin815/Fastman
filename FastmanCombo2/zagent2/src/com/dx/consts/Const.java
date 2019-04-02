package com.dx.consts;

import com.dx.utils.MD5Util;
import com.dx.utils.ZEnvironmentUtil;

public class Const {
    public static final String TAG = "Const";

    public static final String SDCARD_PATH = "ZAgent2";
    public static final String SDCARD_DIR_ROOT = ZEnvironmentUtil.getExternalDirectory(SDCARD_PATH);
    public static final String SDCARD_DIR_LOG = SDCARD_DIR_ROOT + "/log";

    public static final String LOAD_ACTIVITY_ADB = "am start -n com.dx.agent2/.LoadActivity -f 0x10400000";

    public static final String IS_LOG = "is_log";
    public static final String NEW_LOG = "new_log";

    /**
     * 当UrlImei与PhoneImei不相同时，修改URL地址
     * 
     * @param oldUrl
     * @return newUrl
     */
    public static String updateUrl(String phoneImei, String urlImei, String oldUrl) {
        // step1:替换IMEI
        oldUrl = oldUrl.replace(urlImei, phoneImei.replaceAll(",", "%2c"));
        // step2:获取需要加密的字符串
        String str = oldUrl.substring(oldUrl.indexOf("?agg_id=") + 1, oldUrl.indexOf("&brand="));
        // step3:MD5加密5遍字符串
        String newKey = str;
        for (int i = 1; i < 6; i++) {
            newKey = MD5Util.MD5Encode(newKey, true);
        }
        // step4:获取旧的KEY值
        String oldKey = oldUrl.substring(oldUrl.indexOf("&key=") + 5, oldUrl.indexOf("&cmd="));
        // step5:替换KEY值
        return oldUrl.replace(oldKey, newKey);
    }
}
