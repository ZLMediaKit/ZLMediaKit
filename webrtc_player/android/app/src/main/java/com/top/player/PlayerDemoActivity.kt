package com.top.player

import android.os.Bundle
import android.view.View
import android.widget.Toast
import androidx.appcompat.app.AppCompatActivity
import com.rtc.core.ZLMRTCPlayer
import com.rtc.core.play.Status
import com.rtc.core.play.ZLMRTCPlayerImpl
import com.top.player.databinding.ActivityPlayerBinding


class PlayerDemoActivity : AppCompatActivity() {


    private val player: ZLMRTCPlayer by lazy {
        ZLMRTCPlayerImpl(this)
    }


    private val binding by lazy {
        ActivityPlayerBinding.inflate(layoutInflater)
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)

        setContentView(binding.root)

        //ffmpeg -re -stream_loop -1 -i "D:\li\hot\data\data\baseline.mp4" -vcodec h264 -acodec aac -f rtsp -rtsp_transport tcp -bf 0 rtsp://zlmediakit.com/live/li
        //ffmpeg -re -stream_loop -1 -i "D:\li\hot\data\data\test.mp4" -vcodec h264 -acodec aac -f flv -bf 0 rtmp://zlmediakit.com/live/li

        setTitle("Player Demo")
        player.bind(binding.surfaceViewRender)

        player.setOnErrorListener { code, msg ->
            Toast.makeText(this, "code:$code,msg:${msg}", Toast.LENGTH_SHORT).show()

        }

        player.setOnStatusListener {
            when (it) {
                Status.PREPARING -> {
                    binding.tvStatus.text = "准备播放"
                }

                Status.PLAYING -> {
                    binding.tvStatus.text = "播放中.."
                }

                Status.PAUSE -> {
                    binding.tvStatus.text = "暂停中.."
                }

                Status.RESUME -> {
                    binding.tvStatus.text = "播放中.."
                }

                Status.STOP -> {
                    binding.tvStatus.text = ""
                }

                Status.ERROR -> {
                    binding.tvStatus.text = "播放异常"
                }

                else -> {}
            }
        }

    }


    override fun onDestroy() {
        super.onDestroy()
        player.stop()
    }

    fun onPlayClick(view: View) {

        player.play(binding.tvApp.text.toString(), binding.tvStreamId.text.toString())
    }

    fun onPauseClick(view: View) {
        player.pause()
        //Toast.makeText(this, "ok", Toast.LENGTH_SHORT).show()
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
        player.setVolume(0.0f)
    }
}