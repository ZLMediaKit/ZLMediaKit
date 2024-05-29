package com.zlmediakit.webrtc

import android.os.Bundle
import android.view.View
import androidx.appcompat.app.AppCompatActivity
import com.zlm.rtc.ZLMRTCPusher
import com.zlm.rtc.push.ZLMRTCPusherImpl
import kotlinx.android.synthetic.main.activity_player.tv_app
import kotlinx.android.synthetic.main.activity_player.tv_stream_id

class PusherDemoActivity: AppCompatActivity()  {


    private val pusher: ZLMRTCPusher by lazy {
        ZLMRTCPusherImpl(this)
    }


    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)

        setContentView(R.layout.activity_pusher)

    }

    fun onPushCamera(view: View) {



        pusher.push(tv_app.text.toString(), tv_stream_id.text.toString())
    }


    override fun onDestroy() {
        super.onDestroy()
        pusher.stop()
    }
}