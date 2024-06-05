package com.top.player

import android.Manifest
import android.content.Intent
import android.os.Bundle
import android.view.View
import android.widget.Toast
import androidx.appcompat.app.AppCompatActivity
import com.permissionx.guolindev.PermissionX
import com.rtc.core.RTCSurfaceView
import com.rtc.core.ZLMRTCPusher
import com.rtc.core.push.PushMode
import com.rtc.core.push.ZLMRTCPusherImpl
import com.top.player.databinding.ActivityPlayerBinding
import com.top.player.databinding.ActivityPusherBinding


class PusherDemoActivity : AppCompatActivity() {


    private val pusher: ZLMRTCPusher by lazy {
        ZLMRTCPusherImpl(this)
    }


    private val binding by lazy {
        ActivityPusherBinding.inflate(layoutInflater)
    }


    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)

        setContentView(R.layout.activity_pusher)

        val rtcSurfaceView = findViewById<RTCSurfaceView>(R.id.surface_view_render)

        pusher.bind(rtcSurfaceView, true)
        setTitle("Pusher Demo")


        pusher.setOnErrorListener { code, msg ->
            Toast.makeText(this, "code:${code},msg:${msg}",    Toast.LENGTH_SHORT).show()
        }

    }

    fun onPushCamera(view: View) {
        PermissionX.init(this)
            .permissions(Manifest.permission.CAMERA, Manifest.permission.RECORD_AUDIO)
            .request { allGranted, grantedList, deniedList ->
                if (allGranted) {
                    pusher.push(binding.tvApp.text.toString(), binding.tvStreamId.text.toString())
                }
            }
    }

    fun onPushScreen(view: View) {
        PermissionX.init(this)
            .permissions(Manifest.permission.RECORD_AUDIO)
            .request { allGranted, grantedList, deniedList ->
                if (allGranted) {
                    pusher.push(
                        binding.tvApp.text.toString(),
                        binding.tvStreamId.text.toString(),
                        PushMode.SCREEN
                    )
                }
            }
    }

    fun onPushFile(view: View) {
        PermissionX.init(this)
            .permissions(Manifest.permission.WRITE_EXTERNAL_STORAGE,Manifest.permission.READ_EXTERNAL_STORAGE)
            .request { allGranted, grantedList, deniedList ->
                if (allGranted) {
                    pusher.push(
                        binding.tvApp.text.toString(),
                        binding.tvStreamId.text.toString(),
                        PushMode.FILE,
                        ""
                    )
                }
            }
    }

    override fun onDestroy() {
        super.onDestroy()
        pusher.stop()
    }

    fun onStopPush(view: View) {
        pusher.stop()
    }


}