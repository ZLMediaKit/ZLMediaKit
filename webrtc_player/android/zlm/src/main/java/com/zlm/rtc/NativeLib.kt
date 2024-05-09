package com.zlm.rtc

class NativeLib {

    /**
     * A native method that is implemented by the 'rtc' native library,
     * which is packaged with this application.
     */
    external fun stringFromJNI(): String

    companion object {
        // Used to load the 'rtc' library on application startup.
        init {
            System.loadLibrary("rtc")
        }
    }
}