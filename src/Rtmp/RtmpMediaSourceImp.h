/*
 * Copyright (c) 2016 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/xia-chu/ZLMediaKit).
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
class RtmpMediaSourceImp: public RtmpMediaSource, public TrackListener , public MultiMediaSourceMuxer::Listener {
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
    void onWrite(RtmpPacket::Ptr pkt, bool = true) override {
        if (!_all_track_ready || _muxer->isEnabled()) {
            //未获取到所有Track后，或者开启转协议，那么需要解复用rtmp
            _demuxer->inputRtmp(pkt);
        }
        RtmpMediaSource::onWrite(std::move(pkt));
    }

    /**
     * 获取观看总人数，包括(hls/rtsp/rtmp)
     */
    int totalReaderCount() override{
        return readerCount() + (_muxer ? _muxer->totalReaderCount() : 0);
    }

    /**
     * 设置协议转换
     * @param enableHls  是否转换成hls
     * @param enableMP4  是否mp4录制
     */
    void setProtocolTranslation(bool enableHls, bool enableMP4) {
        //不重复生成rtmp
        _muxer = std::make_shared<MultiMediaSourceMuxer>(getVhost(), getApp(), getId(), _demuxer->getDuration(), true, false, enableHls, enableMP4);
        _muxer->setMediaListener(getListener());
        _muxer->setTrackListener(static_pointer_cast<RtmpMediaSourceImp>(shared_from_this()));
        //让_muxer对象拦截一部分事件(比如说录像相关事件)
        MediaSource::setListener(_muxer);

        for(auto &track : _demuxer->getTracks(false)){
            _muxer->addTrack(track);
            track->addDelegate(_muxer);
        }
    }

    /**
     * _demuxer触发的添加Track事件
     */
    void addTrack(const Track::Ptr &track) override {
        if(_muxer){
            _muxer->addTrack(track);
            track->addDelegate(_muxer);
        }
    }

    /**
     * _demuxer触发的Track添加完毕事件
     */
    void addTrackCompleted() override {
        if (_muxer) {
            _muxer->addTrackCompleted();
        }
    }

    void resetTracks() override {
        if (_muxer) {
            _muxer->resetTracks();
        }
    }

    /**
     * _muxer触发的所有Track就绪的事件
     */
    void onAllTrackReady() override{
        _all_track_ready = true;

        if (_recreate_metadata) {
            //更新metadata
            for (auto &track : _muxer->getTracks()) {
                Metadata::addTrack(_metadata, track);
            }
            RtmpMediaSource::updateMetaData(_metadata);
        }
    }

    /**
     * 设置事件监听器
     * @param listener 监听器
     */
    void setListener(const std::weak_ptr<MediaSourceEvent> &listener) override{
        if (_muxer) {
            //_muxer对象不能处理的事件再给listener处理
            _muxer->setMediaListener(listener);
        } else {
            //未创建_muxer对象，事件全部给listener处理
            MediaSource::setListener(listener);
        }
    }

private:
    bool _all_track_ready = false;
    bool _recreate_metadata = false;
    AMFValue _metadata;
    RtmpDemuxer::Ptr _demuxer;
    MultiMediaSourceMuxer::Ptr _muxer;

};
} /* namespace mediakit */

#endif /* SRC_RTMP_RTMPTORTSPMEDIASOURCE_H_ */
