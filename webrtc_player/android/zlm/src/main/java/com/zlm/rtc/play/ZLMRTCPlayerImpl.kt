package com.zlm.rtc.play

import android.content.Context
import android.graphics.Bitmap
import com.zlm.rtc.ZLMRTCPlayer
import org.webrtc.AudioSource
import org.webrtc.AudioTrack
import org.webrtc.EglBase
import org.webrtc.PeerConnection
import org.webrtc.PeerConnectionFactory
import org.webrtc.SurfaceTextureHelper
import org.webrtc.SurfaceViewRenderer
import org.webrtc.VideoCapturer
import org.webrtc.VideoSource
import org.webrtc.VideoTrack

class ZLMRTCPlayerImpl: ZLMRTCPlayer() {

    private val context: Context? = null

    private val eglBase: EglBase? by lazy {
        EglBase.create()
    }
    private var playUrl: String? = null
    private var peerConnection: PeerConnection? = null
    private var surfaceViewRenderer: SurfaceViewRenderer? = null
    private var peerConnectionFactory: PeerConnectionFactory? = null
    private var audioSource: AudioSource? = null
    private var videoSource: VideoSource? = null
    private var localAudioTrack: AudioTrack? = null
    private var localVideoTrack: VideoTrack? = null
    private var captureAndroid: VideoCapturer? = null
    private var surfaceTextureHelper: SurfaceTextureHelper? = null
    private var isShowCamera = true
    private var isPublishMode = false //isPublish true为推流 false为拉流
    private var defaultFps = 24
    private var isPreviewing = false
    private var isFirst = true


    init {

    }


    override fun bind(surface: SurfaceViewRenderer, localPreview: Boolean) {
        this.surfaceViewRenderer = surface
        surfaceViewRenderer?.init(eglBase?.eglBaseContext,null)
    }

    override fun play(app: String, streamId: String) {

    }

    override fun setSpeakerphoneOn(on: Boolean) {

    }

    override fun setLocalMute(on: Boolean) {

    }

    override fun stop() {

    }

    override fun pause() {

    }

    override fun resume() {

    }

    override fun capture(listener: (bitmap: Bitmap) -> Unit) {

    }

    override fun record(record_duration: Long, result: (path: String) -> Unit) {

    }


}