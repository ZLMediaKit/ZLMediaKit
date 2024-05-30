package com.zlm.rtc.push

import android.app.Activity
import android.content.Context
import android.content.Intent
import android.media.projection.MediaProjection
import android.media.projection.MediaProjectionManager
import android.util.Log
import androidx.core.content.ContextCompat
import androidx.fragment.app.FragmentActivity
import com.zlm.rtc.NativeLib
import com.zlm.rtc.ZLMRTCPusher
import com.zlm.rtc.base.ActivityLauncher
import com.zlm.rtc.client.HttpClient
import com.zlm.rtc.client.PeerConnectionClient
import org.json.JSONObject
import org.webrtc.Camera1Enumerator
import org.webrtc.Camera2Enumerator
import org.webrtc.CameraEnumerator
import org.webrtc.EglBase
import org.webrtc.FileVideoCapturer
import org.webrtc.IceCandidate
import org.webrtc.PeerConnectionFactory
import org.webrtc.ScreenCapturerAndroid
import org.webrtc.SessionDescription
import org.webrtc.StatsReport
import org.webrtc.SurfaceViewRenderer
import org.webrtc.VideoCapturer
import java.math.BigInteger
import kotlin.random.Random

class ZLMRTCPusherImpl(val context: FragmentActivity) : ZLMRTCPusher(),
    PeerConnectionClient.PeerConnectionEvents {


    private var peerConnectionClient: PeerConnectionClient? = null

    private var eglBase: EglBase? = null

    private var surfaceViewRenderer: SurfaceViewRenderer? = null

    private var localHandleId = BigInteger.valueOf(Random(2048).nextLong())

    private var app: String = ""
    private var streamId: String = ""


    private val CAPTURE_PERMISSION_REQUEST_CODE = 1


    private fun initPeerConnectionClient(): PeerConnectionClient {
        eglBase = EglBase.create()
        return PeerConnectionClient(
            context, eglBase,
            PeerConnectionClient.PeerConnectionParameters(
                true,
                false,
                false,
                0,
                0,
                0,
                0,
                "VP8",
                true,
                false,
                0,
                "OPUS",
                true,
                false,
                false,
                true,
                false,
                false,
                false,
                false, false, false, null
            ), this
        )
    }

    private fun createVideoCapture(context: Context?): VideoCapturer? {
        val videoCapturer: VideoCapturer? = if (Camera2Enumerator.isSupported(context)) {
            createCameraCapture(Camera2Enumerator(context))
        } else {
            createCameraCapture(Camera1Enumerator(true))
        }

        return videoCapturer
    }


    private fun createScreenCapture(context: Context?): VideoCapturer? {
        val videoCapturer: VideoCapturer? = if (Camera2Enumerator.isSupported(context)) {
            createCameraCapture(Camera2Enumerator(context))
        } else {
            createCameraCapture(Camera1Enumerator(true))
        }

        return videoCapturer
    }


    /**
     * 创建相机媒体流
     */
    private fun createCameraCapture(enumerator: CameraEnumerator): VideoCapturer? {
        val deviceNames = enumerator.deviceNames

        // Front facing camera not found, try something else
        for (deviceName in deviceNames) {
            if (enumerator.isFrontFacing(deviceName)) {
                val videoCapturer: VideoCapturer? = enumerator.createCapturer(deviceName, null)
                if (videoCapturer != null) {
                    return videoCapturer
                }
            }
        }
        // First, try to find front facing camera
        for (deviceName in deviceNames) {
            if (enumerator.isFrontFacing(deviceName)) {
                val videoCapturer: VideoCapturer? = enumerator.createCapturer(deviceName, null)
                if (videoCapturer != null) {
                    return videoCapturer
                }
            }
        }


        return null
    }

    private fun logger(msg: String) {
        Log.i("ZLMRTCPusherImpl", msg)
    }



    override fun bind(surface: SurfaceViewRenderer, localPreview: Boolean) {
        this.surfaceViewRenderer = surface
    }


    override fun push(app: String, streamId: String, mode: PushMode, inputFile: String) {
        this.app = app
        this.streamId = streamId
        if (peerConnectionClient == null) peerConnectionClient = initPeerConnectionClient()
        surfaceViewRenderer?.init(eglBase?.eglBaseContext, null)
        peerConnectionClient?.setAudioEnabled(true)
        peerConnectionClient?.setVideoEnabled(true)
        peerConnectionClient?.createPeerConnectionFactory(PeerConnectionFactory.Options())

        if (mode == PushMode.CAMERA) {
            peerConnectionClient?.createPeerConnection(createVideoCapture(context), localHandleId)
            peerConnectionClient?.createOffer(localHandleId)

        } else if (mode == PushMode.SCREEN) {

            val mediaProjectionManager = context.getSystemService(
                Context.MEDIA_PROJECTION_SERVICE
            ) as MediaProjectionManager

            ActivityLauncher.init(context).startActivityForResult(
                mediaProjectionManager.createScreenCaptureIntent()
            ) { resultCode, data ->
                if (resultCode == Activity.RESULT_OK) {
                    ContextCompat.startForegroundService(context, Intent(context, ScreenShareService::class.java))
                    val screenCapturerAndroid =
                        ScreenCapturerAndroid(data, object : MediaProjection.Callback() {

                        })
                    peerConnectionClient?.createPeerConnection(screenCapturerAndroid, localHandleId)
                    peerConnectionClient?.createOffer(localHandleId)
                }
            }

        } else {
            peerConnectionClient?.createPeerConnection(FileVideoCapturer(inputFile), localHandleId)
            peerConnectionClient?.createOffer(localHandleId)
        }
    }

    override fun stop() {
        surfaceViewRenderer?.clearImage()
        surfaceViewRenderer?.release()
        peerConnectionClient?.stopVideoSource()
        peerConnectionClient?.close()
        peerConnectionClient = null
    }

    override fun onLocalDescription(handleId: BigInteger?, sdp: SessionDescription?) {
        val url = NativeLib().makePushUrl(app, streamId)
        logger("handleId: $url")
        logger("handleId: " + sdp?.description)
        val doPost = HttpClient.doPost(
            url,
            mutableMapOf(Pair("sdp", sdp?.description)),
            mutableMapOf()
        )
        val result = JSONObject(doPost)
        logger(result.toString())
        val code = result.getInt("code")
        if (code == 0) {
            logger("handleId: $doPost")
            val sdp = result.getString("sdp")
            peerConnectionClient?.setRemoteDescription(
                handleId,
                SessionDescription(SessionDescription.Type.ANSWER, sdp)
            )
        } else {
            val msg = result.getString("msg")
            logger("handleId: $msg")
            peerConnectionClient?.setAudioEnabled(false)
            peerConnectionClient?.setVideoEnabled(false)
        }
    }

    override fun onIceCandidate(handleId: BigInteger?, candidate: IceCandidate?) {
    }

    override fun onIceCandidatesRemoved(
        handleId: BigInteger?,
        candidates: Array<out IceCandidate>?
    ) {

    }

    override fun onIceConnected(handleId: BigInteger?) {

    }

    override fun onIceDisconnected(handleId: BigInteger?) {

    }

    override fun onPeerConnectionClosed(handleId: BigInteger?) {

    }

    override fun onPeerConnectionStatsReady(
        handleId: BigInteger?,
        reports: Array<out StatsReport>?
    ) {

    }

    override fun onPeerConnectionError(handleId: BigInteger?, description: String?) {

    }

    override fun onLocalRender(handleId: BigInteger?) {
        if (handleId == localHandleId) {
            peerConnectionClient?.setVideoRender(handleId, surfaceViewRenderer)
        }
    }

    override fun onRemoteRender(handleId: BigInteger?) {

    }

}