/*
 * Copyright (c) 2016 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/xiongziliang/ZLMediaKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#include "FFmpegSource.h"
#include "Common/config.h"
#include "Common/MediaSource.h"
#include "Util/File.h"
#include "System.h"
#include "Thread/WorkThreadPool.h"

namespace FFmpeg {
#define FFmpeg_FIELD "ffmpeg."
const string kBin = FFmpeg_FIELD"bin";
const string kCmd = FFmpeg_FIELD"cmd";
const string kLog = FFmpeg_FIELD"log";
const string kSnap = FFmpeg_FIELD"snap";

onceToken token([]() {
#ifdef _WIN32
    string ffmpeg_bin = trim(System::execute("where ffmpeg"));
#else
    string ffmpeg_bin = trim(System::execute("which ffmpeg"));
#endif
    //默认ffmpeg命令路径为环境变量中路径
    mINI::Instance()[kBin] = ffmpeg_bin.empty() ? "ffmpeg" : ffmpeg_bin;
    //ffmpeg日志保存路径
    mINI::Instance()[kLog] = "./ffmpeg/ffmpeg.log";
    mINI::Instance()[kCmd] = "%s -re -i %s -c:a aac -strict -2 -ar 44100 -ab 48k -c:v libx264 -f flv %s";
    mINI::Instance()[kSnap] = "%s -i %s -y -f mjpeg -t 0.001 %s";
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
    _process.run(cmd,ffmpeg_log.empty() ? "" : File::absolutePath("",ffmpeg_log));
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
                                 _media_info._streamid);
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
                strongSelf->play(strongSelf->_src_url, strongSelf->_dst_url, timeout_ms, [weakSelf](const SockException &ex) {
                    if(!ex){
                        //没有错误
                        return;
                    }
                    auto strongSelf = weakSelf.lock();
                    if (!strongSelf) {
                        //自身已经销毁
                        return;
                    }
                    //上次重试时间超过10秒，那么再重试FFmpeg拉流
                    strongSelf->startTimer(10 * 1000);
                });
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

int FFmpegSource::totalReaderCount(MediaSource &sender) {
    auto listener = _listener.lock();
    if(listener){
        return listener->totalReaderCount(sender);
    }
    return sender.readerCount();
}

void FFmpegSource::onNoneReader(MediaSource &sender){
    auto listener = _listener.lock();
    if(listener){
        listener->onNoneReader(sender);
        return;
    }
    MediaSourceEvent::onNoneReader(sender);
}

void FFmpegSource::onRegist(MediaSource &sender, bool regist){
    auto listener = _listener.lock();
    if(listener){
        listener->onRegist(sender, regist);
    }
}

void FFmpegSource::onGetMediaSource(const MediaSource::Ptr &src) {
    _listener = src->getListener();
    src->setListener(shared_from_this());
}

void FFmpegSnap::makeSnap(const string &play_url, const string &save_path, float timeout_sec,  const function<void(bool)> &cb) {
    GET_CONFIG(string,ffmpeg_bin,FFmpeg::kBin);
    GET_CONFIG(string,ffmpeg_snap,FFmpeg::kSnap);
    GET_CONFIG(string,ffmpeg_log,FFmpeg::kLog);

    std::shared_ptr<Process> process = std::make_shared<Process>();
    auto delayTask = EventPollerPool::Instance().getPoller()->doDelayTask(timeout_sec * 1000,[process,cb](){
        if(process->wait(false)){
            //FFmpeg进程还在运行，超时就关闭它
            process->kill(2000);
        }
        return 0;
    });

    WorkThreadPool::Instance().getPoller()->async([process,play_url,save_path,delayTask,cb](){
        char cmd[1024] = {0};
        snprintf(cmd, sizeof(cmd),ffmpeg_snap.data(),ffmpeg_bin.data(),play_url.data(),save_path.data());
        process->run(cmd,ffmpeg_log.empty() ? "" : File::absolutePath("",ffmpeg_log));
        //等待FFmpeg进程退出
        process->wait(true);
        //FFmpeg进程退出了可以取消定时器了
        delayTask->cancel();
        //执行回调函数
        cb(process->exit_code() == 0);
    });
}

