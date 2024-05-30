package com.zlmediakit.webrtc

import android.Manifest
import android.content.Intent
import android.content.pm.PackageManager
import android.os.Bundle
import android.view.View
import androidx.appcompat.app.AppCompatActivity
import androidx.core.app.ActivityCompat
import androidx.core.content.ContextCompat
import com.permissionx.guolindev.PermissionX
import com.permissionx.guolindev.callback.RequestCallback
import com.zlm.rtc.ZLMRTCPusher
import com.zlm.rtc.push.PushMode
import com.zlm.rtc.push.ZLMRTCPusherImpl
import kotlinx.android.synthetic.main.activity_player.surface_view_renderer
import kotlinx.android.synthetic.main.activity_player.tv_app
import kotlinx.android.synthetic.main.activity_player.tv_stream_id

class PusherDemoActivity : AppCompatActivity() {


    private val pusher: ZLMRTCPusher by lazy {
        ZLMRTCPusherImpl(this)
    }


    override fun onActivityResult(requestCode: Int, resultCode: Int, data: Intent?) {
        super.onActivityResult(requestCode, resultCode, data)
    }
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)

        setContentView(R.layout.activity_pusher)

        pusher.bind(surface_view_renderer, true)

    }

    fun onPushCamera(view: View) {
        PermissionX.init(this)
            .permissions(Manifest.permission.CAMERA, Manifest.permission.RECORD_AUDIO)
            .request { allGranted, grantedList, deniedList ->
                if (allGranted) {
                    pusher.push(tv_app.text.toString(), tv_stream_id.text.toString())
                }
            }
    }

    fun onPushScreen(view: View) {
        PermissionX.init(this)
            .permissions(Manifest.permission.RECORD_AUDIO)
            .request { allGranted, grantedList, deniedList ->
                if (allGranted) {
                    pusher.push(
                        tv_app.text.toString(),
                        tv_stream_id.text.toString(),
                        PushMode.SCREEN
                    )
                }
            }
    }

    fun onPushFile(view: View) {

    }

    override fun onDestroy() {
        super.onDestroy()
        pusher.stop()
    }


}