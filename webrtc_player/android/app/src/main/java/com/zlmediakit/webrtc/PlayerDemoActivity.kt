package com.zlmediakit.webrtc

import android.os.Bundle
import androidx.appcompat.app.AppCompatActivity
import com.zlm.rtc.ZLMRTCPlayer
import kotlinx.android.synthetic.main.activity_player.surface_view_renderer

class PlayerDemoActivity:AppCompatActivity() {

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)

        setContentView(R.layout.activity_play)

        ZLMRTCPlayer.shareInstance().bind(this,surface_view_renderer,true)


    }
}