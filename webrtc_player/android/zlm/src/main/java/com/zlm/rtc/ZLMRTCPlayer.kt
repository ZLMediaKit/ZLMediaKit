package com.zlm.rtc

import android.content.Context
import android.graphics.Bitmap
import com.zlm.rtc.play.ZLMRTCPlayerImpl
import org.webrtc.SurfaceViewRenderer

abstract class ZLMRTCPlayer {

    public abstract fun bind(surface: SurfaceViewRenderer)

    //拉流接口
    public abstract fun play(app: String, streamId: String)

    public abstract fun setVolume()

    public abstract fun stop()

    public abstract fun pause()

    public abstract fun resume()

    public abstract fun capture(listener: (bitmap: Bitmap) -> Unit)

    public abstract fun record(duration: Long, result: (path: String) -> Unit)

}