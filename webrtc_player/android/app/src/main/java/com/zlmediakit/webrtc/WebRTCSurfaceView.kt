package com.zlmediakit.webrtc

import android.content.Context
import android.graphics.Bitmap
import android.media.AudioManager
import android.util.AttributeSet
import android.util.Log
import android.view.LayoutInflater
import android.widget.RelativeLayout
import androidx.lifecycle.DefaultLifecycleObserver
import androidx.lifecycle.LifecycleOwner
import com.google.gson.Gson
import okhttp3.*
import okhttp3.MediaType.Companion.toMediaType
import okhttp3.MediaType.Companion.toMediaTypeOrNull
import org.webrtc.*
import org.webrtc.RendererCommon.ScalingType
import org.webrtc.audio.AudioDeviceModule
import org.webrtc.audio.JavaAudioDeviceModule
import java.io.IOException
import java.util.*

public class WebRTCSurfaceView(context: Context, attrs: AttributeSet?) :
    RelativeLayout(context, attrs), DefaultLifecycleObserver, RendererCommon.RendererEvents {


    private data class sdp(var sdp: String, var username: String, var password: String)

    private data class SdpResponse(var code: Int, var id: String, var sdp: String, var type: String)

    private enum class ErrorCode(val errorCode: Int) {
        SUCCESS(0x00),
        GET_REMOTE_SDP_ERROR(0x01);
    }


    companion object {
        private val TAG = "WebRTCSurfaceView"

    }

    private var mContext: Context = context

    private val eglBase: EglBase = EglBase.create()
    private var mEGLBaseContext: EglBase.Context = eglBase.eglBaseContext

    private lateinit var videoUrl: String;

    private var mPeerConnectionFactory: PeerConnectionFactory? = null

    private var mLocalMediaStream: MediaStream? = null
    private var mLocalAudioTrack: AudioTrack? = null
    private var mAudioSource: AudioSource? = null

    private var mLocalSessionDescription: SessionDescription? = null
    private var mRemoteSessionDescription: SessionDescription? = null

    private var mLocalPeer: Peer? = null

    private var mSurfaceViewRenderer: SurfaceViewRenderer

    private lateinit var OnErrorListener: (errorCode: Int, errorMsg: String) -> Unit?

    fun setOnErrorListener(listener: (errorCode: Int, errorMsg: String) -> Unit) {
        this.OnErrorListener = listener
    }

    private lateinit var OnPreparedListener: () -> Unit?

    fun setOnPreparedListener(listener: () -> Unit) {
        this.OnPreparedListener = listener
    }

    private val audioManager: AudioManager


    init {

        val view = LayoutInflater.from(mContext).inflate(R.layout.layout_videoview, this)

        mPeerConnectionFactory = createConnectionFactory()

        mSurfaceViewRenderer = view.findViewById(R.id.surface_view_renderer)

        mSurfaceViewRenderer.init(mEGLBaseContext, this)
        mSurfaceViewRenderer.setScalingType(ScalingType.SCALE_ASPECT_FILL)
        mSurfaceViewRenderer.setEnableHardwareScaler(true)


        //创建媒体流
        mLocalMediaStream = mPeerConnectionFactory?.createLocalMediaStream("ARDAMS")
        //采集音频
        mAudioSource = mPeerConnectionFactory?.createAudioSource(createAudioConstraints())
        mLocalAudioTrack = mPeerConnectionFactory?.createAudioTrack("ARDAMSa0", mAudioSource)

        //添加Tracks
        mLocalMediaStream?.addTrack(mLocalAudioTrack)

        audioManager = context.getSystemService(Context.AUDIO_SERVICE) as AudioManager
        audioManager.isSpeakerphoneOn = false


    }


    private fun set(width: Int, height: Int) {
        layoutParams.width = width
        layoutParams.height = height
    }

    private fun createConnectionFactory(): PeerConnectionFactory? {

        val options = PeerConnectionFactory.InitializationOptions.builder(mContext)
            .setEnableInternalTracer(false)
            .createInitializationOptions()

        PeerConnectionFactory.initialize(options)

        val videoEncoderFactory = DefaultVideoEncoderFactory(
            mEGLBaseContext,
            true,
            true
        )

        val videoDecoderFactory = DefaultVideoDecoderFactory(mEGLBaseContext)


        val audioDevice = createJavaAudioDevice()
        val peerConnectionFactory = PeerConnectionFactory.builder()
            .setAudioDeviceModule(audioDevice)
            .setVideoEncoderFactory(videoEncoderFactory)
            .setVideoDecoderFactory(videoDecoderFactory)
            .createPeerConnectionFactory()
        audioDevice.release()

        return peerConnectionFactory

    }

    private fun createAudioConstraints(): MediaConstraints {
        val audioConstraints = MediaConstraints()
        audioConstraints.mandatory.add(
            MediaConstraints.KeyValuePair(
                "googEchoCancellation",
                "true"
            )
        )
        audioConstraints.mandatory.add(
            MediaConstraints.KeyValuePair(
                "googAutoGainControl",
                "false"
            )
        )
        audioConstraints.mandatory.add(
            MediaConstraints.KeyValuePair(
                "googHighpassFilter",
                "true"
            )
        )
        audioConstraints.mandatory.add(
            MediaConstraints.KeyValuePair(
                "googNoiseSuppression",
                "true"
            )
        )
        return audioConstraints
    }

    private fun offerOrAnswerConstraint(): MediaConstraints {
        val mediaConstraints = MediaConstraints()
        val keyValuePairs = java.util.ArrayList<MediaConstraints.KeyValuePair>()
        keyValuePairs.add(MediaConstraints.KeyValuePair("OfferToReceiveAudio", "true"))
        keyValuePairs.add(MediaConstraints.KeyValuePair("OfferToReceiveVideo", "true"))
        mediaConstraints.mandatory.addAll(keyValuePairs)
        return mediaConstraints
    }

    private fun createJavaAudioDevice(): AudioDeviceModule {
        val audioTrackErrorCallback: JavaAudioDeviceModule.AudioTrackErrorCallback = object :
            JavaAudioDeviceModule.AudioTrackErrorCallback {
            override fun onWebRtcAudioTrackInitError(errorMessage: String) {
                Log.i(TAG, "onWebRtcAudioTrackInitError ============> $errorMessage")

            }

            override fun onWebRtcAudioTrackStartError(
                errorCode: JavaAudioDeviceModule.AudioTrackStartErrorCode, errorMessage: String
            ) {
                Log.i(TAG, "onWebRtcAudioTrackStartError ============> $errorCode:$errorMessage")

            }

            override fun onWebRtcAudioTrackError(errorMessage: String) {
                Log.i(TAG, "onWebRtcAudioTrackError ============> $errorMessage")

            }
        }


        // Set audio track state callbacks.
        val audioTrackStateCallback: JavaAudioDeviceModule.AudioTrackStateCallback = object :
            JavaAudioDeviceModule.AudioTrackStateCallback {
            override fun onWebRtcAudioTrackStart() {
                Log.i(TAG, "onWebRtcAudioTrackStart ============>")

            }

            override fun onWebRtcAudioTrackStop() {
                Log.i(TAG, "onWebRtcAudioTrackStop ============>")

            }
        }

        return JavaAudioDeviceModule.builder(mContext)
            .setUseHardwareAcousticEchoCanceler(true)
            .setUseHardwareNoiseSuppressor(true)
            .setAudioTrackErrorCallback(audioTrackErrorCallback)
            .setAudioTrackStateCallback(audioTrackStateCallback)
            .setUseStereoOutput(true) //立体声
            .createAudioDeviceModule()
    }

    fun setVideoPath(url: String) {
        videoUrl = url
    }

    fun start() {

        mLocalPeer = Peer {
            val okHttpClient = OkHttpClient.Builder().build()


            val body = RequestBody.create("text/plain; charset=utf-8".toMediaType(), it!!)


            val request: Request = Request.Builder()
                .url(videoUrl)
                .post(body)
                .build()

            val call: Call = okHttpClient.newCall(request)

            call.enqueue(object : Callback {
                override fun onFailure(call: Call, e: IOException) {
                    Log.i(TAG, "onFailure")
                    OnErrorListener?.invoke(
                        ErrorCode.GET_REMOTE_SDP_ERROR.errorCode,
                        e.message.toString()
                    )
                }

                override fun onResponse(call: Call, response: Response) {
                    val body = response.body?.string()
                    val sdpResponse = Gson().fromJson(body, SdpResponse::class.java)

                    try {
                        mRemoteSessionDescription = SessionDescription(
                            SessionDescription.Type.fromCanonicalForm("answer"),
                            sdpResponse.sdp
                        )
                        Log.i(
                            TAG,
                            "RemoteSdpObserver onCreateSuccess:[SessionDescription[type=${mRemoteSessionDescription?.type?.name},description=${mRemoteSessionDescription?.description}]]"
                        )
                        mLocalPeer?.setRemoteDescription(mRemoteSessionDescription!!)
                    } catch (e: Exception) {
                        Log.i(TAG, e.toString())
                        OnErrorListener.invoke(
                            ErrorCode.GET_REMOTE_SDP_ERROR.errorCode,
                            e.localizedMessage
                        )
                    }
                }
            })
        }
    }

    fun pause() {
        mSurfaceViewRenderer.pauseVideo()
        //mSurfaceViewRenderer.disableFpsReduction()
    }

    fun resume() {
        mSurfaceViewRenderer.setFpsReduction(15f)
    }

    fun screenshot(listener: (bitmap: Bitmap) -> Unit) {
        mSurfaceViewRenderer.addFrameListener({
            listener.invoke(it)
        }, 1f)
    }

    fun setSpeakerphoneOn(on: Boolean) {
        audioManager.isSpeakerphoneOn = on
    }

    fun mute(on:Boolean) {
        audioManager.isMicrophoneMute=on
    }

    override fun onDestroy(owner: LifecycleOwner) {
        super.onDestroy(owner)
        mSurfaceViewRenderer.release()
        mLocalPeer?.mPeerConnection?.dispose()
        mAudioSource?.dispose()
        mPeerConnectionFactory?.dispose()
    }

    override fun onMeasure(widthMeasureSpec: Int, heightMeasureSpec: Int) {
        super.onMeasure(widthMeasureSpec, heightMeasureSpec)

    }

    inner class Peer(var sdp: (String?) -> Unit = {}) : PeerConnection.Observer, SdpObserver {

        var mPeerConnection: PeerConnection? = null

        init {
            mPeerConnection = createPeerConnection()
            mPeerConnection?.createOffer(this, offerOrAnswerConstraint())
        }

        //初始化 RTCPeerConnection 连接管道
        private fun createPeerConnection(): PeerConnection? {
            if (mPeerConnectionFactory == null) {
                mPeerConnectionFactory = createConnectionFactory()
            }
            // 管道连接抽象类实现方法
            val ICEServers = LinkedList<PeerConnection.IceServer>()
            val rtcConfig = PeerConnection.RTCConfiguration(ICEServers)
            //修改模式 PlanB无法使用仅接收音视频的配置
            //rtcConfig.sdpSemantics = PeerConnection.SdpSemantics.PLAN_B
            return mPeerConnectionFactory?.createPeerConnection(rtcConfig, this)
        }

        fun setRemoteDescription(sdp: SessionDescription) {
            mPeerConnection?.setRemoteDescription(this, sdp)
        }

        override fun onCreateSuccess(sessionDescription: SessionDescription?) {
            mPeerConnection?.setLocalDescription(this, sessionDescription)
            mPeerConnection?.addStream(mLocalMediaStream)
            sdp.invoke(sessionDescription?.description)
        }

        override fun onSetSuccess() {

        }

        override fun onCreateFailure(p0: String?) {

        }

        override fun onSetFailure(p0: String?) {

        }

        override fun onSignalingChange(signalingState: PeerConnection.SignalingState?) {
            Log.i(TAG, "onSignalingChange ============> " + signalingState.toString())
        }

        override fun onIceConnectionChange(iceConnectionState: PeerConnection.IceConnectionState?) {
            Log.i(TAG, "onIceConnectionChange ============> " + iceConnectionState.toString())

        }

        override fun onIceConnectionReceivingChange(p0: Boolean) {
            Log.i(TAG, "onIceConnectionReceivingChange ============> $p0")

        }

        override fun onIceGatheringChange(iceGatheringState: PeerConnection.IceGatheringState?) {
            Log.i(TAG, "onIceGatheringChange ============> ${iceGatheringState.toString()}")
        }

        override fun onIceCandidate(iceCandidate: IceCandidate?) {
            Log.i(TAG, "onIceCandidate ============> ${iceCandidate.toString()}")


        }

        override fun onIceCandidatesRemoved(p0: Array<out IceCandidate>?) {
            Log.i(TAG, "onIceCandidatesRemoved ============> ${p0.toString()}")
        }

        override fun onAddStream(mediaStream: MediaStream?) {
            Log.i(TAG, "onAddStream ============> ${mediaStream?.toString()}")

            if (mediaStream?.videoTracks?.isEmpty() != true) {
                val remoteVideoTrack = mediaStream?.videoTracks?.get(0)
                remoteVideoTrack?.setEnabled(true)
                remoteVideoTrack?.addSink(mSurfaceViewRenderer)
            }

            if (mediaStream?.audioTracks?.isEmpty() != true) {
                val remoteAudioTrack = mediaStream?.audioTracks?.get(0)
                remoteAudioTrack?.setEnabled(true)
                remoteAudioTrack?.setVolume(1.0)
            }


        }

        override fun onRemoveStream(mediaStream: MediaStream?) {
            Log.i(TAG, "onRemoveStream ============> ${mediaStream.toString()}")

        }

        override fun onDataChannel(dataChannel: DataChannel?) {
            Log.i(TAG, "onDataChannel ============> ${dataChannel.toString()}")

        }

        override fun onRenegotiationNeeded() {
            Log.i(TAG, "onRenegotiationNeeded ============>")

        }

        override fun onAddTrack(rtpReceiver: RtpReceiver?, p1: Array<out MediaStream>?) {
            Log.i(TAG, "onAddTrack ============>" + rtpReceiver?.track())
            Log.i(TAG, "onAddTrack ============>" + p1?.size)

        }
    }

    override fun onFirstFrameRendered() {
        Log.i(TAG, "onFirstFrameRendered ============>")

    }

    override fun onFrameResolutionChanged(frameWidth: Int, frameHeight: Int, rotation: Int) {
        Log.i(TAG, "onFrameResolutionChanged ============> $frameWidth:$frameHeight:$rotation")
        //set(frameWidth,frameHeight)
    }




}