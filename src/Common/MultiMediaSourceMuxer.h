/*
 * Copyright (c) 2016 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/xiongziliang/ZLMediaKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#ifndef ZLMEDIAKIT_MULTIMEDIASOURCEMUXER_H
#define ZLMEDIAKIT_MULTIMEDIASOURCEMUXER_H

#include "Rtsp/RtspMediaSourceMuxer.h"
#include "Rtmp/RtmpMediaSourceMuxer.h"
#include "Record/Recorder.h"
#include "Record/HlsMediaSource.h"
#include "Record/HlsRecorder.h"

/**
 * 使用该对象时，应该使用setListener方法来绑定MediaSource相关的事件
 * 否则多种不同类型的MediaSource(rtsp/rtmp/hls)将无法产生关联
 */
class MultiMediaSourceMuxer : public MediaSink , public std::enable_shared_from_this<MultiMediaSourceMuxer>{
public:
    class Listener{
    public:
        Listener() = default;
        virtual ~Listener() = default;
        virtual void onAllTrackReady() = 0;
    };

    typedef std::shared_ptr<MultiMediaSourceMuxer> Ptr;
    MultiMediaSourceMuxer(const string &vhost, const string &app, const string &stream, float dur_sec = 0.0,
                          bool enable_rtsp = true, bool enable_rtmp = true, bool enable_hls = true, bool enable_mp4 = false){
        if (enable_rtmp) {
            _rtmp = std::make_shared<RtmpMediaSourceMuxer>(vhost, app, stream, std::make_shared<TitleMeta>(dur_sec));
        }
        if (enable_rtsp) {
            _rtsp = std::make_shared<RtspMediaSourceMuxer>(vhost, app, stream, std::make_shared<TitleSdp>(dur_sec));
        }

        if(enable_hls){
            _hls = Recorder::createRecorder(Recorder::type_hls,vhost, app, stream);
        }

        if(enable_mp4){
            _mp4 = Recorder::createRecorder(Recorder::type_mp4,vhost, app, stream);
        }
    }
    virtual ~MultiMediaSourceMuxer(){}

    /**
     * 重置音视频媒体
     */
    void resetTracks() override{
        if(_rtmp){
            _rtmp->resetTracks();
        }
        if(_rtsp){
            _rtsp->resetTracks();
        }
        if(_hls){
            _hls->resetTracks();
        }
        if(_mp4){
            _mp4->resetTracks();
        }
    }

    /**
     * 设置事件监听器
     * @param listener
     */
    void setListener(const std::weak_ptr<MediaSourceEvent> &listener){
        if(_rtmp) {
            _rtmp->setListener(listener);
        }

        if(_rtsp) {
            _rtsp->setListener(listener);
        }

        auto hls_src = getHlsMediaSource();
        if(hls_src){
            hls_src->setListener(listener);
        }
    }

    /**
     * 返回总的消费者个数
     * @return
     */
    int totalReaderCount() const{
        auto hls_src = getHlsMediaSource();
        return (_rtsp ? _rtsp->readerCount() : 0) + (_rtmp ? _rtmp->readerCount() : 0) + (hls_src ? hls_src->readerCount() : 0);
    }

    void setTimeStamp(uint32_t stamp){
        if(_rtmp){
            _rtmp->setTimeStamp(stamp);
        }
        if(_rtsp){
            _rtsp->setTimeStamp(stamp);
        }
    }

    void setTrackListener(Listener *listener){
        _listener = listener;
    }
protected:
    /**
     * 添加音视频媒体
     * @param track 媒体描述
     */
    void onTrackReady(const Track::Ptr & track) override {
        if(_rtmp){
            _rtmp->addTrack(track);
        }
        if(_rtsp){
            _rtsp->addTrack(track);
        }
        if(_hls){
            _hls->addTrack(track);
        }
        if(_mp4){
            _mp4->addTrack(track);
        }
    }

    /**
     * 写入帧数据然后打包rtmp
     * @param frame 帧数据
     */
    void onTrackFrame(const Frame::Ptr &frame) override {
        if(_rtmp) {
            _rtmp->inputFrame(frame);
        }
        if(_rtsp) {
            _rtsp->inputFrame(frame);
        }
        if(_hls){
            _hls->inputFrame(frame);
        }
        if(_mp4){
            _mp4->inputFrame(frame);
        }
    }

    /**
     * 所有Track都准备就绪，触发媒体注册事件
     */
    void onAllTrackReady() override{
        if(_rtmp) {
            _rtmp->setTrackSource(shared_from_this());
            _rtmp->onAllTrackReady();
        }
        if(_rtsp) {
            _rtsp->setTrackSource(shared_from_this());
            _rtsp->onAllTrackReady();
        }

        auto hls_src = getHlsMediaSource();
        if(hls_src){
            hls_src->setTrackSource(shared_from_this());
        }

        if(_listener){
            _listener->onAllTrackReady();
        }
    }

    MediaSource::Ptr getHlsMediaSource() const{
        auto recorder = dynamic_pointer_cast<HlsRecorder>(_hls);
        if(recorder){
            return recorder->getMediaSource();
        }
        return nullptr;
    }
private:
    RtmpMediaSourceMuxer::Ptr _rtmp;
    RtspMediaSourceMuxer::Ptr _rtsp;
    MediaSinkInterface::Ptr _hls;
    MediaSinkInterface::Ptr _mp4;
    Listener *_listener = nullptr;
};


#endif //ZLMEDIAKIT_MULTIMEDIASOURCEMUXER_H
