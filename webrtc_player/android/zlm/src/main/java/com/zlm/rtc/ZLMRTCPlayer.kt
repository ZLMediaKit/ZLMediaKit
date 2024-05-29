package com.zlm.rtc

import android.content.Context
import android.graphics.Bitmap
import com.zlm.rtc.play.ZLMRTCPlayerImpl
import org.webrtc.SurfaceViewRenderer

abstract class ZLMRTCPlayer {

//    companion object {
//        fun shareInstance(): ZLMRTCPlayer {
//            return ZLMRTCPlayerImpl(this)
//        }
//    }





    constructor()

    public abstract fun bind(surface: SurfaceViewRenderer, localPreview:Boolean)


    //拉流接口
    public abstract fun play(app: String, streamId: String)

    public abstract fun setSpeakerphoneOn(on: Boolean)

    public abstract fun setLocalMute(on: Boolean)


    public abstract fun stop()

    public abstract fun pause()



    public abstract fun resume()

    public abstract fun capture(listener: (bitmap: Bitmap) -> Unit)

    public abstract fun record(duration: Long, result: (path: String) -> Unit)


    //推流接口
//    public abstract fun startLocalPreview()
//
//    public abstract fun stopLocalPreview()
//
//    public abstract fun startPublishing()
//
//    public abstract fun stopPublishing()


    //

}