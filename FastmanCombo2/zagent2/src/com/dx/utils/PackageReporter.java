package com.dx.utils;

import java.io.ByteArrayInputStream;
import java.io.InputStream;
import java.security.cert.CertificateFactory;
import java.security.cert.X509Certificate;
import java.util.ArrayList;
import java.util.Collections;
import java.util.Comparator;
import java.util.List;

import android.content.Context;
import android.content.pm.ApplicationInfo;
import android.content.pm.PackageInfo;
import android.content.pm.PackageManager;
import android.content.pm.Signature;

public class PackageReporter {
    final static String TAG = "PackageReporter";

    public static class Package {
        public String packageName = "";
        public String name = "unknown";
        public int versionCode = 0;
        public String versionName = "";
        public String sourceDir = "unknown";
        public int flags = 0;
        public boolean enabled = false;
        public List<String> signatures;

        public boolean hasSignature(String sig) {
            return signatures.contains(sig);
        }

        public boolean hasSignature(List<String> sigs) {
            boolean ret = false;
            for (String sig : sigs) {
                if (signatures.contains(sig)) {
                    ret = true;
                    break;
                }
            }
            return ret;
        }

        public void dump() {
            Log.i(TAG, ArrayJoin(signatures, "/") + "  " + packageName);
        }
    }

    PackageManager pm;
    List<Package> packageList;
    List<String> android_signature;

    public PackageReporter(Context context) {
        pm = context.getPackageManager();
        packageList = new ArrayList<Package>();
        android_signature = new ArrayList<String>();
    }

    class PackageSort implements Comparator<Package> {
        public int compare(Package lhs, Package rhs) {
            return ArrayJoin(lhs.signatures, ",").compareTo(ArrayJoin(rhs.signatures, ","));
        }
    }

    public List<Package> loadPackageList() {
        packageList.clear();

        List<Package> tmpList = new ArrayList<Package>();
        List<PackageInfo> list = pm.getInstalledPackages(PackageManager.GET_SIGNATURES);
        for (PackageInfo info : list) {
            if (info.applicationInfo == null) {
                continue;
            }

            try {
                Package pkg = new Package();
                pkg.packageName = info.packageName;

                pkg.name = info.applicationInfo.loadLabel(pm).toString();
                pkg.sourceDir = info.applicationInfo.sourceDir;
                pkg.flags = info.applicationInfo.flags;
                pkg.enabled = info.applicationInfo.enabled;

                pkg.versionCode = info.versionCode;
                pkg.versionName = info.versionName.replaceAll("[,:]", "_");
                pkg.signatures = getPkgSignature(info.signatures);

                if (pkg.packageName.equals("android") // android
                        || pkg.packageName.equals("com.android.contacts")
                        || pkg.packageName.equals("com.android.calendar")
                        || pkg.packageName.equals("com.android.camera")
                        || pkg.packageName.equals("com.android.mms")
                        || pkg.packageName.equals("com.android.browser")
                        || pkg.packageName.equals("com.android.providers.downloads")
                        || pkg.packageName.equals("com.android.providers.contacts")
                        || pkg.packageName.equals("com.android.providers.calendar")

                        || pkg.packageName.equals("com.google.android.gsf") // google
                        || pkg.packageName.equals("com.google.android.gms")
                        || pkg.packageName.equals("com.android.vending")
                        || pkg.packageName.equals("com.google.android.apps.maps")

                        || pkg.packageName.contains("infraware.polarisoffice")
                        || pkg.packageName.contains("diotek.diodict")
                        || pkg.packageName.startsWith("com.arcsoft.")
                        || pkg.packageName.startsWith("com.hp.android.")

                        || pkg.packageName.startsWith("com.samsung.") // samsung
                        || pkg.packageName.startsWith("com.sec.android.")
                        || pkg.packageName.equals("com.sec.spp.push")
                        || pkg.packageName.equals("com.android.smspush")
                        || pkg.packageName.equals("com.osp.app.signin")
                        || pkg.packageName.equals("tv.peel.samsung.app")

                        || pkg.packageName.equals("com.lenovo.leos.cloud.sync") // lenovo
                        || pkg.packageName.equals("com.lenovo.launcher") || pkg.packageName.equals("com.htc.calendar") // htc

                        || pkg.packageName.startsWith("com.icoolme.android.") // coolpad
                        || pkg.packageName.equals("com.osa.ktouchpay") // ktouch
                ) {
                    android_signature.removeAll(pkg.signatures);
                    android_signature.addAll(pkg.signatures);
                    continue;
                }

                tmpList.add(pkg);
            } catch (Exception e) {
                e.printStackTrace();
            }
        }

        for (String sig : android_signature) {
            Log.i(TAG, "ignored signature: " + sig);
        }

        for (Package pkg : tmpList) {
            if (!pkg.hasSignature(android_signature)) {
                packageList.add(pkg);
            }
        }

        Collections.sort(packageList, new PackageSort());
        for (Package pkg : packageList) {
            pkg.dump();
        }
        Log.i(TAG, "loadPackageList done, count=" + packageList.size());
        return packageList;
    }

    private List<String> getPkgSignature(Signature[] signatures) throws Exception {
        List<String> ret = new ArrayList<String>();
        for (Signature signature : signatures) {
            byte[] data = signature.toByteArray();
            if (data.length == 0) {
                ret.add("_EMPTY_");
                continue;
            }

            InputStream ins = new ByteArrayInputStream(data);
            CertificateFactory factory = CertificateFactory.getInstance("X509");
            X509Certificate cert = (X509Certificate) factory.generateCertificate(ins);
            ret.add(cert.getSerialNumber().toString());
        }
        return ret;
    }

    private static String ArrayJoin(List<String> list, String splitter) {
        StringBuilder sb = new StringBuilder();
        boolean isSecond = false;
        for (String s : list) {
            if (isSecond) {
                sb.append(splitter);
            }
            isSecond = true;
            sb.append(s);
        }
        return sb.toString();
    }

    private static int APP_TYPE_UNKNOWN = -1;
    private static int APP_TYPE_SYSTEM_APP = 0;
    private static int APP_TYPE_UPDATED_SYSTEM_APP = 1;
    private static int APP_TYPE_USER_APP = 3;

    /**
     * 
     * @param flags
     * @return
     */
    public static int getApplicationType(ApplicationInfo ai) {
        if (ai != null) {
            if ((ai.flags & ApplicationInfo.FLAG_UPDATED_SYSTEM_APP) != 0) {
                return APP_TYPE_UPDATED_SYSTEM_APP;
            } else if ((ai.flags & ApplicationInfo.FLAG_SYSTEM) == 0) {
                return APP_TYPE_USER_APP;
            } else {
                return APP_TYPE_SYSTEM_APP;
            }
        }
        return APP_TYPE_UNKNOWN;
    }
}
