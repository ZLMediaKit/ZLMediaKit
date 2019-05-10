package com.zlmediakit.jni;

public class ZLMediaKit {
    static {
        System.loadLibrary("zlmediakit_jni");
    }
    static public native boolean startDemo(String sd_path);
}
