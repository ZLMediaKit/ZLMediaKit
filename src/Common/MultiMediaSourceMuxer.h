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
            Recorder::startRecord(Recorder::type_hls,vhost, app, stream, "", true, false);
        }

        if(enable_mp4){
            Recorder::startRecord(Recorder::type_mp4,vhost, app, stream, "", true, false);
        }

        _get_hls_media_source = [vhost,app,stream](){
            auto recorder = dynamic_pointer_cast<HlsRecorder>(Recorder::getRecorder(Recorder::type_hls,vhost,app,stream));
            if(recorder){
                return recorder->getMediaSource();
            }
            return MediaSource::Ptr();
        };
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

        auto hls_src = _get_hls_media_source();
        if(hls_src){
            hls_src->setListener(listener);
        }
    }

    /**
     * 返回总的消费者个数
     * @return
     */
    int totalReaderCount() const{
        auto hls_src = _get_hls_media_source();
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

        auto hls_src = _get_hls_media_source();
        if(hls_src){
            hls_src->setTrackSource(shared_from_this());
        }

        if(_listener){
            _listener->onAllTrackReady();
        }
    }
private:
    RtmpMediaSourceMuxer::Ptr _rtmp;
    RtspMediaSourceMuxer::Ptr _rtsp;
    Listener *_listener = nullptr;
    function<MediaSource::Ptr ()> _get_hls_media_source;
};


#endif //ZLMEDIAKIT_MULTIMEDIASOURCEMUXER_H
