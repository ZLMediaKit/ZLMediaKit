package com.zlm.rtc

import android.graphics.Bitmap
import com.zlm.rtc.push.PushMode
import org.webrtc.SurfaceViewRenderer

abstract class ZLMRTCPusher {

    public abstract fun bind(surface: SurfaceViewRenderer, localPreview: Boolean)

    abstract fun push(app: String, streamId: String, mode: PushMode = PushMode.CAMERA)

    abstract fun stop()

}