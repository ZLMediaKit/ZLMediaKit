package com.zlmediakit.webrtc

import android.os.Bundle
import android.os.Handler
import androidx.appcompat.app.AppCompatActivity
import com.zlm.rtc.ZLMRTCPlayer
import kotlinx.android.synthetic.main.activity_player.surface_view_renderer

class PlayerDemoActivity:AppCompatActivity() {

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)

        setContentView(R.layout.activity_player)

        ZLMRTCPlayer.shareInstance().bind(applicationContext,surface_view_renderer,true)


        Handler().postDelayed({
           ZLMRTCPlayer.shareInstance().play("live","li")
        },1000)

    }


    override fun onDestroy() {
        super.onDestroy()
        ZLMRTCPlayer.shareInstance().destroy()
    }
}