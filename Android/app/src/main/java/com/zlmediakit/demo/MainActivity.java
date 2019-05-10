package com.zlmediakit.demo;

import android.content.Intent;
import android.content.pm.PackageManager;
import android.os.Environment;
import android.support.v4.app.ActivityCompat;
import android.support.v7.app.AppCompatActivity;
import android.os.Bundle;
import android.widget.Toast;

import com.zlmediakit.jni.ZLMediaKit;

public class MainActivity extends AppCompatActivity {
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

        String ini = Environment.getExternalStoragePublicDirectory("").toString();
        if(permissionSuccess){
            Toast.makeText(this,"你可以修改配置文件再启动：" + ini + "/zlmediakit.jni" ,Toast.LENGTH_LONG).show();
            ZLMediaKit.startDemo(ini);
        }else{
            Toast.makeText(this,"请给予我权限，否则无法启动测试！" ,Toast.LENGTH_LONG).show();
        }
    }

}
