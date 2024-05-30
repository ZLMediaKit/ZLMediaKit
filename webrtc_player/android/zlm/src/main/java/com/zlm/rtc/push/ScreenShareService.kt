package com.zlm.rtc.push

import android.app.NotificationChannel
import android.app.NotificationManager
import android.app.Service
import android.content.Intent
import android.graphics.BitmapFactory
import android.media.projection.MediaProjectionManager
import android.os.Build
import android.os.IBinder
import androidx.core.app.NotificationCompat
import com.zlm.rtc.R

class ScreenShareService :Service(){
    override fun onBind(intent: Intent?): IBinder? {
        return null
    }

    override fun onCreate() {
        super.onCreate()
    }

    override fun onStartCommand(intent: Intent?, flags: Int, startId: Int): Int {
        val resultCode = intent?.getIntExtra("resultCode", -1)
        val resultData = intent?.getParcelableExtra<Intent>("data")
        createNotificationChannel()

        resultCode?.let {code->
            resultData?.let { data->
                val mediaProjectionManager =  getSystemService(MEDIA_PROJECTION_SERVICE) as MediaProjectionManager
                val mediaProjection = mediaProjectionManager.getMediaProjection(code, data);
            }
        }


        return super.onStartCommand(intent, flags, startId)
    }

    override fun onDestroy() {
        super.onDestroy()
    }



    private fun createNotificationChannel() {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            val channel = NotificationChannel(
                "ScreenShared", "屏幕录制",
                NotificationManager.IMPORTANCE_DEFAULT
            )
            val manager = getSystemService(
                NotificationManager::class.java
            )
            manager.createNotificationChannel(channel)
        }

        val notification = NotificationCompat.Builder(this, "ScreenShared")
            .setContentTitle("屏幕分享")
            .setContentText("分享中...")
            .setSmallIcon(R.drawable.icon_screen_share)
            .setLargeIcon(BitmapFactory.decodeResource(resources, R.drawable.icon_screen_share))
            .setAutoCancel(true)
            .build()
        startForeground(1024, notification)
    }
}