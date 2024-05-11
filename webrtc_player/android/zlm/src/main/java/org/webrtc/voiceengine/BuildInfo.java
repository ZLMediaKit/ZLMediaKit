//
// Source code recreated from a .class file by IntelliJ IDEA
// (powered by FernFlower decompiler)
//

package org.webrtc.voiceengine;

import android.os.Build;
import android.os.Build.VERSION;

public final class BuildInfo {
    public BuildInfo() {
    }

    public static String getDevice() {
        return Build.DEVICE;
    }

    public static String getDeviceModel() {
        return Build.MODEL;
    }

    public static String getProduct() {
        return Build.PRODUCT;
    }

    public static String getBrand() {
        return Build.BRAND;
    }

    public static String getDeviceManufacturer() {
        return Build.MANUFACTURER;
    }

    public static String getAndroidBuildId() {
        return Build.ID;
    }

    public static String getBuildType() {
        return Build.TYPE;
    }

    public static String getBuildRelease() {
        return VERSION.RELEASE;
    }

    public static int getSdkVersion() {
        return VERSION.SDK_INT;
    }
}
