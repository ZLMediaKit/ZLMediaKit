package com.zlmediakit.webrtc

import android.os.Bundle
import android.view.View
import android.widget.Toast
import androidx.appcompat.app.AppCompatActivity
import com.zlm.rtc.ZLMRTCPlayer
import com.zlm.rtc.play.ZLMRTCPlayerImpl
import kotlinx.android.synthetic.main.activity_player.surface_view_renderer
import kotlinx.android.synthetic.main.activity_player.tv_app
import kotlinx.android.synthetic.main.activity_player.tv_stream_id

class PlayerDemoActivity : AppCompatActivity() {


    private val player: ZLMRTCPlayer by lazy {
        ZLMRTCPlayerImpl(this)
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)

        setContentView(R.layout.activity_player)

        //ffmpeg -re -stream_loop -1 -i "D:\li\hot\data\data\baseline.mp4" -vcodec h264 -acodec aac -f rtsp -rtsp_transport tcp -bf 0 rtsp://zlmediakit.com/live/li
        //ffmpeg -re -stream_loop -1 -i "D:\li\hot\data\data\test.mp4" -vcodec h264 -acodec aac -f flv -bf 0 rtmp://zlmediakit.com/live/li

        player.bind(surface_view_renderer, false)

    }


    override fun onDestroy() {
        super.onDestroy()
        player.stop()
    }

    fun onPlayClick(view: View) {

        player.play(tv_app.text.toString(), tv_stream_id.text.toString())
    }

    fun onPauseClick(view: View) {
        player.pause()
    }

    fun onStopClick(view: View) {
        player.stop()
    }

    fun onResumeClick(view: View) {
        player.resume()
    }

    fun onCapture(view: View) {
        player.capture {
            Toast.makeText(this, "capture ok", Toast.LENGTH_SHORT).show()
        }
    }

    fun onRecord(view: View) {
        player.record(10 * 1000) {
            Toast.makeText(this, "" + it, Toast.LENGTH_SHORT).show()
        }
    }

    fun onVolume(view: View) {
        player.setSpeakerphoneOn(true)
    }
}