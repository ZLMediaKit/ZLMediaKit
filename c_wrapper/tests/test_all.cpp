/*
 * MIT License
 *
 * Copyright (c) 2016 xiongziliang <771730766@qq.com>
 *
 * This file is part of ZLMediaKit(https://github.com/xiongziliang/ZLMediaKit).
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */
#include "mediakit.h"
#include <unistd.h>
#include <signal.h>


void *media = nullptr;

int main(int argc,char *argv[]){
    //设置退出信号处理函数
    signal(SIGINT, [](int){});
    ////////init environment////////////////////////////////////////////////////////////////
    onAppStart();
    ////////init service/////////////////////////////////////////////////////////////////////
    initHttpServer(80);
    initRtmpServer(1935);
    initRtspServer(554);
    ////////ProxyPlayer////////////////////////////////////////////////////////////////////////
    auto proxy = createProxyPlayer("app","proxy",0);
    proxyPlayer_play(proxy,"rtmp://live.hkstv.hk.lxdns.com/live/hks");
    log_warn("%s","请打开vlc播放:rtsp://127.0.0.1/app/proxy  "
            "rtmp://127.0.0.1/app/proxy  "
            "http://127.0.0.1/app/proxy/hls.m3u8");
    ////////http downloader////////////////////////////////////////////////////////////////////
    auto downloader = createDownloader();
    downloader_startDownload(downloader,
                             "http://pic4.nipic.com/20091121/3764872_215617048242_2.jpg",
                             [](void *userData,int code,const char *errMsg,const char *filePath){
                                 log_info("下载结果:%d-%s:%s",code,errMsg,filePath);
    }, downloader);
    ////////player//////////////////////////////////////////////////////////////////////////////
    auto player = createPlayer();

    player_setOnPlayResult(player,[](void *userData,int errCode,const char *errMsg){
        log_info("播放结果:%d-%s",errCode,errMsg);

        ////////media/////////////////////////////////////////////////////////////////////////////////
        if(errCode){
            //play failed
            return;
        }
        auto player = userData;
        media = createMedia("app","media");
        if(player_containAudio(player) == 1){
            media_initAudio(media,
                            player_getAudioChannel(player),
                            player_getAudioSampleBit(player),
                            player_getAudioSampleRate(player));
        }

        if(player_containVideo(player) == 1){
            media_initVideo(media,
                            player_getVideoWidth(player),
                            player_getVideoHeight(player),
                            player_getVideoFps(player));
        }
        log_warn("%s","请打开vlc播放:rtsp://127.0.0.1/app/media  "
                "rtmp://127.0.0.1/app/media  "
                "http://127.0.0.1/app/media/hls.m3u8");
    },player);

    player_setOnShutdown(player,[](void *userData,int errCode,const char *errMsg){
        log_info("播放器异常断开:%d-%s",errCode,errMsg);

        if(media){
            releaseMedia(media);
        }
    },player);

    player_setOnGetAudio(player,[](void *userData,void *data,int len,unsigned long timeStamp){
        //在此解码视频
        //TO-DO
        //log_trace("audio:%d-%d",len,timeStamp);

        ////////输入aac///////////
        if(media){
            media_inputAAC(media,data,len,timeStamp);
        }
    },player);

    player_setOnGetVideo(player,[](void *userData,void *data,int len,unsigned long dts,unsigned long pts){
        //在此解码音频
        //TO-DO
        //log_trace("video:%d-%d-%d",len,dts,pts);

        ////////输入264///////////
        if(media){
            media_inputH264(media,data,len,dts);
        }
    },player);

    player_play(player,"rtmp://live.hkstv.hk.lxdns.com/live/hks");


    //sleep forever
    sleep(UINT32_MAX);
    ////////uninit environment////////////////////////////////////////////////////////////////////
    onAppExit();
    return 0;
}

