package com.zlm.rtc

class NativeLib {

    /**
     * A native method that is implemented by the 'rtc' native library,
     * which is packaged with this application.
     */
    external fun stringFromJNI(): String

    external fun exchangeSessionDescription(description:String): String

    external fun makeUrl(app:String,streamId:String): String


    companion object {
        // Used to load the 'rtc' library on application startup.
        init {
            System.loadLibrary("rtc")
        }
    }
}