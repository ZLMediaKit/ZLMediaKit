package com.zlm.rtc

import android.graphics.Bitmap

abstract class ZLMRTCPusher {
    abstract fun push(app: String, streamId: String)

    abstract fun stop()

}