package com.zlm.rtc

import android.graphics.Bitmap

abstract class ZLMRTCPush {

    constructor()

    public abstract fun init(serverUrl: String)

    //拉流接口
    public abstract fun play(app: String, streamId: String)

    public abstract fun setSpeakerphoneOn(on: Boolean)

    public abstract fun setLocalMute(on: Boolean)


    public abstract fun stop()

    public abstract fun pause()

    public abstract fun resume()

    public abstract fun capture(listener: (bitmap: Bitmap) -> Unit)

    public abstract fun record(record_duration: Long, result: (path: String) -> Unit)





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