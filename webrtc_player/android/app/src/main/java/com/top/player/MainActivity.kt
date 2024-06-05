package com.top.player

import android.content.Intent
import android.os.Bundle
import android.view.View
import androidx.appcompat.app.AppCompatActivity

class MainActivity : AppCompatActivity() {

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.activity_main)

    }

    fun toPlayActivity(view: View) {
        startActivity(Intent(this, PlayerDemoActivity::class.java))
    }

    fun toPushActivity(view: View) {
        startActivity(Intent(this, PusherDemoActivity::class.java))

    }

}