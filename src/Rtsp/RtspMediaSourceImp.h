/*
 * Copyright (c) 2016 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/xiongziliang/ZLMediaKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#ifndef SRC_RTSP_RTSPTORTMPMEDIASOURCE_H_
#define SRC_RTSP_RTSPTORTMPMEDIASOURCE_H_

#include "Rtmp/amf.h"
#include "RtspMediaSource.h"
#include "RtspDemuxer.h"
#include "Common/MultiMediaSourceMuxer.h"
using namespace toolkit;

namespace mediakit {
class RtspMediaSourceImp : public RtspMediaSource, public Demuxer::Listener , public MultiMediaSourceMuxer::Listener  {
public:
    typedef std::shared_ptr<RtspMediaSourceImp> Ptr;

    /**
     * 构造函数
     * @param vhost 虚拟主机
     * @param app 应用名
     * @param id 流id
     * @param ringSize 环形缓存大小
     */
    RtspMediaSourceImp(const string &vhost, const string &app, const string &id, int ringSize = RTP_GOP_SIZE) : RtspMediaSource(vhost, app, id,ringSize) {
        _demuxer = std::make_shared<RtspDemuxer>();
        _demuxer->setTrackListener(this);
    }

    ~RtspMediaSourceImp() = default;

    /**
     * 设置sdp
     */
    void setSdp(const string &strSdp) override {
        _demuxer->loadSdp(strSdp);
        RtspMediaSource::setSdp(strSdp);
    }

    /**
     * 输入rtp并解析
     */
    void onWrite(const RtpPacket::Ptr &rtp, bool key_pos) override {
        if(_all_track_ready && !_muxer->isEnabled()){
            //获取到所有Track后，并且未开启转协议，那么不需要解复用rtp
            //在关闭rtp解复用后，无法知道是否为关键帧，这样会导致无法秒开，或者开播花屏
            key_pos = rtp->type == TrackVideo;
        }else{
            //需要解复用rtp
            key_pos = _demuxer->inputRtp(rtp);
        }
        RtspMediaSource::onWrite(rtp, key_pos);
    }

    /**
     * 设置监听器
     * @param listener
     */
    void setListener(const std::weak_ptr<MediaSourceEvent> &listener) override {
        RtspMediaSource::setListener(listener);
        if(_muxer){
            _muxer->setMediaListener(listener);
        }
    }

    /**
     * 获取观看总人数，包括(hls/rtsp/rtmp)
     */
    int totalReaderCount() override{
        return readerCount() + (_muxer ? _muxer->totalReaderCount() : 0);
    }

    /**
     * 设置录制状态
     * @param type 录制类型
     * @param start 开始或停止
     * @param custom_path 开启录制时，指定自定义路径
     * @return 是否设置成功
     */
    bool setupRecord(Recorder::type type, bool start, const string &custom_path) override{
        if(_muxer){
            return _muxer->setupRecord(*this,type, start, custom_path);
        }
        return RtspMediaSource::setupRecord(type, start, custom_path);
    }

    /**
     * 获取录制状态
     * @param type 录制类型
     * @return 录制状态
     */
    bool isRecording(Recorder::type type) override{
        if(_muxer){
            return _muxer->isRecording(*this,type);
        }
        return RtspMediaSource::isRecording(type);
    }


    /**
     * 设置协议转换
     * @param enableRtmp 是否转换成rtmp
     * @param enableHls  是否转换成hls
     * @param enableMP4  是否mp4录制
     */
    void setProtocolTranslation(bool enableRtmp,bool enableHls,bool enableMP4){
        //不重复生成rtsp
        _muxer = std::make_shared<MultiMediaSourceMuxer>(getVhost(), getApp(), getId(), _demuxer->getDuration(), false, enableRtmp, enableHls, enableMP4);
        _muxer->setMediaListener(getListener());
        _muxer->setTrackListener(this);
        for(auto &track : _demuxer->getTracks(false)){
            _muxer->addTrack(track);
            track->addDelegate(_muxer);
        }
    }

    /**
     * _demuxer触发的添加Track事件
     */
    void onAddTrack(const Track::Ptr &track) override {
        if(_muxer){
            _muxer->addTrack(track);
            track->addDelegate(_muxer);
        }
    }

    /**
     * _muxer触发的所有Track就绪的事件
     */
    void onAllTrackReady() override{
        setTrackSource(_muxer);
        _all_track_ready = true;
    }
private:
    RtspDemuxer::Ptr _demuxer;
    MultiMediaSourceMuxer::Ptr _muxer;
    bool _all_track_ready = false;
};
} /* namespace mediakit */

#endif /* SRC_RTSP_RTSPTORTMPMEDIASOURCE_H_ */
