/*
 * MIT License
 *
 * Copyright (c) 2016-2019 xiongziliang <771730766@qq.com>
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

#include "FFmpegSource.h"
#include "Common/config.h"
#include "Common/MediaSource.h"
#include "Util/File.h"
#include "System.h"

namespace FFmpeg {
#define FFmpeg_FIELD "ffmpeg."
const char kBin[] = FFmpeg_FIELD"bin";
const char kCmd[] = FFmpeg_FIELD"cmd";
const char kLog[] = FFmpeg_FIELD"log";

onceToken token([]() {
    mINI::Instance()[kBin] = trim(System::execute("which ffmpeg"));
    mINI::Instance()[kCmd] = "%s -re -i %s -c:a aac -strict -2 -ar 44100 -ab 48k -c:v libx264 -f flv %s";
    mINI::Instance()[kLog] = "./ffmpeg/ffmpeg.log";
});
}

FFmpegSource::FFmpegSource() {
    _poller = EventPollerPool::Instance().getPoller();
}

FFmpegSource::~FFmpegSource() {
    DebugL;
}


void FFmpegSource::play(const string &src_url,const string &dst_url,int timeout_ms,const onPlay &cb) {
    GET_CONFIG(string,ffmpeg_bin,FFmpeg::kBin);
    GET_CONFIG(string,ffmpeg_cmd,FFmpeg::kCmd);
    GET_CONFIG(string,ffmpeg_log,FFmpeg::kLog);

    _src_url = src_url;
    _dst_url = dst_url;
    _media_info.parse(dst_url);

    char cmd[1024] = {0};
    snprintf(cmd, sizeof(cmd),ffmpeg_cmd.data(),ffmpeg_bin.data(),src_url.data(),dst_url.data());
    _process.run(cmd,File::absolutePath("",ffmpeg_log));
    InfoL << cmd;

    if(_media_info._host == "127.0.0.1"){
        //推流给自己的，通过判断流是否注册上来判断是否正常
        if(_media_info._schema != RTSP_SCHEMA && _media_info._schema != RTMP_SCHEMA){
            cb(SockException(Err_other,"本服务只支持rtmp/rtsp推流"));
            return;
        }
        weak_ptr<FFmpegSource> weakSelf = shared_from_this();
        findAsync(timeout_ms,[cb,weakSelf,timeout_ms](const MediaSource::Ptr &src){
            auto strongSelf = weakSelf.lock();
            if(!strongSelf){
                //自己已经销毁
                return;
            }
            if(src){
                //推流给自己成功
                cb(SockException());
                strongSelf->onGetMediaSource(src);
                strongSelf->startTimer(timeout_ms);
                return;
            }
            //推流失败
            if(!strongSelf->_process.wait(false)){
                //ffmpeg进程已经退出
                cb(SockException(Err_other,StrPrinter << "ffmpeg已经退出,exit code = " << strongSelf->_process.exit_code()));
                return;
            }
            //ffmpeg进程还在线，但是等待推流超时
            cb(SockException(Err_other,"等待超时"));
        });
    } else{
        //推流给其他服务器的，通过判断FFmpeg进程是否在线判断是否成功
        weak_ptr<FFmpegSource> weakSelf = shared_from_this();
        _timer = std::make_shared<Timer>(timeout_ms / 1000,[weakSelf,cb,timeout_ms](){
            auto strongSelf = weakSelf.lock();
            if(!strongSelf){
                //自身已经销毁
                return false;
            }
            //FFmpeg还在线，那么我们认为推流成功
            if(strongSelf->_process.wait(false)){
                cb(SockException());
                strongSelf->startTimer(timeout_ms);
                return false;
            }
            //ffmpeg进程已经退出
            cb(SockException(Err_other,StrPrinter << "ffmpeg已经退出,exit code = " << strongSelf->_process.exit_code()));
            return false;
        },_poller);
    }
}

void FFmpegSource::findAsync(int maxWaitMS, const function<void(const MediaSource::Ptr &src)> &cb) {
    auto src = MediaSource::find(_media_info._schema,
                                 _media_info._vhost,
                                 _media_info._app,
                                 _media_info._streamid,
                                 false);
    if(src || !maxWaitMS){
        cb(src);
        return;
    }

    void *listener_tag = this;
    //若干秒后执行等待媒体注册超时回调
    auto onRegistTimeout = _poller->doDelayTask(maxWaitMS,[cb,listener_tag](){
        //取消监听该事件
        NoticeCenter::Instance().delListener(listener_tag,Broadcast::kBroadcastMediaChanged);
        cb(nullptr);
        return 0;
    });

    weak_ptr<FFmpegSource> weakSelf = shared_from_this();
    auto onRegist = [listener_tag,weakSelf,cb,onRegistTimeout](BroadcastMediaChangedArgs) {
        auto strongSelf = weakSelf.lock();
        if(!strongSelf) {
            //本身已经销毁，取消延时任务
            onRegistTimeout->cancel();
            NoticeCenter::Instance().delListener(listener_tag,Broadcast::kBroadcastMediaChanged);
            return;
        }

        if (!bRegist ||
            sender.getSchema() != strongSelf->_media_info._schema ||
            sender.getVhost() != strongSelf->_media_info._vhost ||
            sender.getApp() != strongSelf->_media_info._app ||
            sender.getId() != strongSelf->_media_info._streamid) {
            //不是自己感兴趣的事件，忽略之
            return;
        }

        //查找的流终于注册上了；取消延时任务，防止多次回调
        onRegistTimeout->cancel();
        //取消事件监听
        NoticeCenter::Instance().delListener(listener_tag,Broadcast::kBroadcastMediaChanged);

        //切换到自己的线程再回复
        strongSelf->_poller->async([listener_tag,weakSelf,cb](){
            auto strongSelf = weakSelf.lock();
            if(!strongSelf) {
                return;
            }
            //再找一遍媒体源，一般能找到
            strongSelf->findAsync(0,cb);
        }, false);
    };
    //监听媒体注册事件
    NoticeCenter::Instance().addListener(listener_tag, Broadcast::kBroadcastMediaChanged, onRegist);
}

/**
 * 定时检查媒体是否在线
 */
void FFmpegSource::startTimer(int timeout_ms) {
    weak_ptr<FFmpegSource> weakSelf = shared_from_this();
    _timer = std::make_shared<Timer>(1, [weakSelf, timeout_ms]() {
        auto strongSelf = weakSelf.lock();
        if (!strongSelf) {
            //自身已经销毁
            return false;
        }
        if (strongSelf->_media_info._host == "127.0.0.1") {
            //推流给自己的，我们通过检查是否已经注册来判断FFmpeg是否工作正常
            strongSelf->findAsync(0, [&](const MediaSource::Ptr &src) {
                //同步查找流
                if (!src) {
                    //流不在线，重新拉流
                    if(strongSelf->_replay_ticker.elapsedTime() > 10 * 1000){
                        //上次重试时间超过10秒，那么再重试FFmpeg拉流
                        strongSelf->_replay_ticker.resetTime();
                        strongSelf->play(strongSelf->_src_url, strongSelf->_dst_url, timeout_ms, [](const SockException &) {});
                    }
                }
            });
        } else {
            //推流给其他服务器的，我们通过判断FFmpeg进程是否在线，如果FFmpeg推流中断，那么它应该会自动退出
            if (!strongSelf->_process.wait(false)) {
                //ffmpeg不在线，重新拉流
                strongSelf->play(strongSelf->_src_url, strongSelf->_dst_url, timeout_ms, [](const SockException &) {});
            }
        }
        return true;
    }, _poller);
}

void FFmpegSource::setOnClose(const function<void()> &cb){
    _onClose = cb;
}

bool FFmpegSource::close(MediaSource &sender, bool force) {
    auto listener = _listener.lock();
    if(listener && !listener->close(sender,force)){
        //关闭失败
        return false;
    }
    //该流无人观看，我们停止吧
    if(_onClose){
        _onClose();
    }
    return true;
}

void FFmpegSource::onNoneReader(MediaSource &sender) {
    auto listener = _listener.lock();
    if(listener){
        listener->onNoneReader(sender);
    }else{
        MediaSourceEvent::onNoneReader(sender);
    }
}

int FFmpegSource::totalReaderCount(MediaSource &sender) {
    auto listener = _listener.lock();
    if(listener){
        return listener->totalReaderCount(sender);
    }
    return 0;
}

void FFmpegSource::onGetMediaSource(const MediaSource::Ptr &src) {
    _listener = src->getListener();
    src->setListener(shared_from_this());
}
