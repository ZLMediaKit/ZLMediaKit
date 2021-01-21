package com.zlmediakit.demo;

import android.content.Intent;
import android.content.pm.PackageManager;
import android.os.Environment;
import android.support.v4.app.ActivityCompat;
import android.support.v7.app.AppCompatActivity;
import android.os.Bundle;
import android.util.Log;
import android.widget.Toast;

import com.zlmediakit.jni.ZLMediaKit;

public class MainActivity extends AppCompatActivity {
    public static final String TAG = "ZLMediaKit";
    private static String[] PERMISSIONS_STORAGE = {
            "android.permission.READ_EXTERNAL_STORAGE",
            "android.permission.WRITE_EXTERNAL_STORAGE",
            "android.permission.INTERNET"};

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);

        boolean permissionSuccess = true;
        for(String str : PERMISSIONS_STORAGE){
            int permission = ActivityCompat.checkSelfPermission(this, str);
            if (permission != PackageManager.PERMISSION_GRANTED) {
                // 没有写的权限，去申请写的权限，会弹出对话框
                ActivityCompat.requestPermissions(this, PERMISSIONS_STORAGE,1);
                permissionSuccess = false;
                break;
            }
        }

        String sd_dir = Environment.getExternalStoragePublicDirectory("").toString();
        if(permissionSuccess){
            Toast.makeText(this,"你可以修改配置文件再启动：" + sd_dir + "/zlmediakit.ini" ,Toast.LENGTH_LONG).show();
            Toast.makeText(this,"SSL证书请放置在：" + sd_dir + "/zlmediakit.pem" ,Toast.LENGTH_LONG).show();
        }else{
            Toast.makeText(this,"请给予我权限，否则无法启动测试！" ,Toast.LENGTH_LONG).show();
        }
        ZLMediaKit.startDemo(sd_dir);
    }

    private ZLMediaKit.MediaPlayer _player;
    private void test_player(){
        _player = new ZLMediaKit.MediaPlayer("rtmp://live.hkstv.hk.lxdns.com/live/hks1", new ZLMediaKit.MediaPlayerCallBack() {
            @Override
            public void onPlayResult(int code, String msg) {
                Log.d(TAG,"onPlayResult:" + code + "," + msg);
            }

            @Override
            public void onShutdown(int code, String msg) {
                Log.d(TAG,"onShutdown:" + code + "," + msg);
            }

            @Override
            public void onData(ZLMediaKit.MediaFrame frame) {
                Log.d(TAG,"onData:"
                        + frame.trackType + ","
                        + frame.codecId + ","
                        + frame.dts + ","
                        + frame.pts + ","
                        + frame.keyFrame + ","
                        + frame.prefixSize + ","
                        + frame.data.length);
            }
        });
    }

}
