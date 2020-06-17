/*
 * Copyright (c) 2016 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/xiongziliang/ZLMediaKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#ifndef SRC_RTMP_RTMPTORTSPMEDIASOURCE_H_
#define SRC_RTMP_RTMPTORTSPMEDIASOURCE_H_

#include <mutex>
#include <string>
#include <memory>
#include <functional>
#include <unordered_map>
#include "Util/util.h"
#include "Util/logger.h"
#include "amf.h"
#include "Rtmp.h"
#include "RtmpMediaSource.h"
#include "RtmpDemuxer.h"
#include "Common/MultiMediaSourceMuxer.h"
using namespace std;
using namespace toolkit;

namespace mediakit {
class RtmpMediaSourceImp: public RtmpMediaSource, public Demuxer::Listener , public MultiMediaSourceMuxer::Listener {
public:
    typedef std::shared_ptr<RtmpMediaSourceImp> Ptr;

    /**
     * 构造函数
     * @param vhost 虚拟主机
     * @param app 应用名
     * @param id 流id
     * @param ringSize 环形缓存大小
     */
    RtmpMediaSourceImp(const string &vhost, const string &app, const string &id, int ringSize = RTMP_GOP_SIZE) : RtmpMediaSource(vhost, app, id, ringSize) {
        _demuxer = std::make_shared<RtmpDemuxer>();
        _demuxer->setTrackListener(this);
    }

    ~RtmpMediaSourceImp() = default;

    /**
     * 设置metadata
     */
    void setMetaData(const AMFValue &metadata) override{
        if(!_demuxer->loadMetaData(metadata)){
            //该metadata无效，需要重新生成
            _metadata = metadata;
            _recreate_metadata = true;
        }
        RtmpMediaSource::setMetaData(metadata);
    }

    /**
     * 输入rtmp并解析
     */
    void onWrite(const RtmpPacket::Ptr &pkt,bool key_pos = true) override {
        if(_all_track_ready && !_muxer->isEnabled()){
            //获取到所有Track后，并且未开启转协议，那么不需要解复用rtmp
            key_pos = pkt->isVideoKeyFrame();
        }else{
            //需要解复用rtmp
            key_pos = _demuxer->inputRtmp(pkt);
        }

        RtmpMediaSource::onWrite(pkt,key_pos);
    }

    /**
     * 设置监听器
     * @param listener
     */
    void setListener(const std::weak_ptr<MediaSourceEvent> &listener) override {
        RtmpMediaSource::setListener(listener);
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
        return RtmpMediaSource::setupRecord(type, start, custom_path);
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
        return RtmpMediaSource::isRecording(type);
    }


    /**
     * 设置协议转换
     * @param enableRtsp 是否转换成rtsp
     * @param enableHls  是否转换成hls
     * @param enableMP4  是否mp4录制
     */
    void setProtocolTranslation(bool enableRtsp, bool enableHls, bool enableMP4) {
        //不重复生成rtmp
        _muxer = std::make_shared<MultiMediaSourceMuxer>(getVhost(), getApp(), getId(), _demuxer->getDuration(), enableRtsp, false, enableHls, enableMP4);
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

        if (_recreate_metadata) {
            //更新metadata
            for (auto &track : _muxer->getTracks()) {
                Metadata::addTrack(_metadata, track);
            }
            RtmpMediaSource::updateMetaData(_metadata);
        }
    }

private:
    RtmpDemuxer::Ptr _demuxer;
    MultiMediaSourceMuxer::Ptr _muxer;
    AMFValue _metadata;
    bool _all_track_ready = false;
    bool _recreate_metadata = false;
};
} /* namespace mediakit */

#endif /* SRC_RTMP_RTMPTORTSPMEDIASOURCE_H_ */
