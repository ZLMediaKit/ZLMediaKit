package com.zlm.rtc.play

import android.content.Context
import android.graphics.Bitmap
import android.util.Log
import com.zlm.rtc.NativeLib
import com.zlm.rtc.ZLMRTCPlayer
import com.zlm.rtc.client.HttpClient
import com.zlm.rtc.client.PeerConnectionClient
import org.json.JSONObject
import org.webrtc.Camera1Enumerator
import org.webrtc.Camera2Enumerator
import org.webrtc.CameraEnumerator
import org.webrtc.EglBase
import org.webrtc.IceCandidate
import org.webrtc.PeerConnectionFactory
import org.webrtc.SessionDescription
import org.webrtc.StatsReport
import org.webrtc.SurfaceViewRenderer
import org.webrtc.VideoCapturer
import java.math.BigInteger

class ZLMRTCPlayerImpl : ZLMRTCPlayer(), PeerConnectionClient.PeerConnectionEvents {

    private var context: Context? = null

    private val peerConnectionClient: PeerConnectionClient? by lazy {
        PeerConnectionClient(
            context, EglBase.create(),
            PeerConnectionClient.PeerConnectionParameters(
                true,
                false,
                false,
                1080,
                960,
                0,
                0,
                "VP8",
                true,
                false,
                0,
                "OPUS",
                false,
                false,
                false,
                false,
                false,
                false,
                false,
                false, false, false, null
            ), this
        )
    }


    init {

    }

    private fun logger(msg: String) {
        Log.i("ZLMRTCPlayerImpl", msg)
    }

    fun createVideoCapture(context: Context?): VideoCapturer? {
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
            if (!enumerator.isFrontFacing(deviceName)) {
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
    override fun bind(context: Context, surface: SurfaceViewRenderer, localPreview: Boolean) {
        this.context = context
        peerConnectionClient?.setAudioEnabled(true)
        peerConnectionClient?.createPeerConnectionFactory(PeerConnectionFactory.Options())
        peerConnectionClient?.createPeerConnection(createVideoCapture(context), BigInteger.ZERO)
        peerConnectionClient?.createOffer((BigInteger.ZERO))


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


    override fun onLocalDescription(handleId: BigInteger?, sdp: SessionDescription?) {

        val url = NativeLib().makeUrl("live", "li")
        logger("handleId: " + url)
        logger("handleId: " + sdp?.description)
        val doPost = HttpClient.doPost(
            url,
            mutableMapOf(Pair("sdp", sdp?.description)),
            mutableMapOf()
        )

        val result = JSONObject(doPost)

        val code = result.getInt("code")
        if (code == 0) {
            logger("handleId: " + doPost)
            val sdp = result.getString("sdp")
            peerConnectionClient?.setRemoteDescription(handleId,SessionDescription(SessionDescription.Type.ANSWER,sdp))
        } else {
            val msg = result.getString("msg")
            logger("handleId: " + msg)
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

    }

    override fun onRemoteRender(handleId: BigInteger?) {

    }


}