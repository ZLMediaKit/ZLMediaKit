package com.zlmediakit.webrtc

import android.annotation.SuppressLint
import android.graphics.drawable.BitmapDrawable
import android.graphics.drawable.Drawable
import android.os.Bundle
import android.widget.Toast
import androidx.appcompat.app.AppCompatActivity
import kotlinx.android.synthetic.main.activity_main.*
import kotlinx.android.synthetic.main.activity_main.view.*


class MainActivity : AppCompatActivity() {

    private var isSpeaker = true

    @SuppressLint("SetTextI18n")
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.activity_main)

        lifecycle.addObserver(web_rtc_sv)

        //http://124.223.98.45/index/api/webrtc?app=live&stream=test&type=play
        url.setText("http://124.223.98.45/index/api/webrtc?app=live&stream=test&type=play")

        //http://192.168.1.17/index/api/webrtc?app=live&stream=test&type=play
        btn_play.setOnClickListener {
            web_rtc_sv?.setVideoPath(url.text.toString())
            web_rtc_sv.start()
        }

        web_rtc_sv.setOnErrorListener { errorCode, errorMsg ->
            runOnUiThread {
                Toast.makeText(this, "errorCode:$errorCode,errorMsg:$errorMsg", Toast.LENGTH_SHORT)
                    .show()
            }
        }


        btn_pause.setOnClickListener {
            web_rtc_sv?.pause()
        }

        btn_resume.setOnClickListener {
            web_rtc_sv?.resume()
        }

        btn_screenshot.setOnClickListener {
            web_rtc_sv?.screenshot {
                runOnUiThread {
                    iv_screen.setImageDrawable(BitmapDrawable(it))
                }
            }
        }

        btn_mute.setOnClickListener {
            web_rtc_sv.mute(true)
        }


        selectAudio()
        btn_speaker.setOnClickListener {
            selectAudio()
        }

    }

    fun selectAudio(){
        if (isSpeaker){
            btn_speaker.setText("扬声器")
            web_rtc_sv.setSpeakerphoneOn(isSpeaker)
        }else{
            btn_speaker.setText("话筒")
            web_rtc_sv.setSpeakerphoneOn(isSpeaker)
        }
        isSpeaker=!isSpeaker
    }
}