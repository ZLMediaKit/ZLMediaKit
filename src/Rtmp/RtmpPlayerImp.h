/*
 * Copyright (c) 2016 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/xia-chu/ZLMediaKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#ifndef SRC_RTMP_RTMPPLAYERIMP_H_
#define SRC_RTMP_RTMPPLAYERIMP_H_

#include <memory>
#include <functional>
#include "Common/config.h"
#include "RtmpPlayer.h"
#include "RtmpMediaSource.h"
#include "RtmpDemuxer.h"
#include "Poller/Timer.h"
#include "Util/TimeTicker.h"

namespace mediakit {

class RtmpPlayerImp: public PlayerImp<RtmpPlayer,PlayerBase>, private TrackListener {
public:
    using Ptr = std::shared_ptr<RtmpPlayerImp>;
    using Super = PlayerImp<RtmpPlayer,PlayerBase>;

    RtmpPlayerImp(const toolkit::EventPoller::Ptr &poller) : Super(poller) {};

    ~RtmpPlayerImp() override {
        DebugL << std::endl;
    }

    float getProgress() const override {
        if (getDuration() > 0) {
            return getProgressMilliSecond() / (getDuration() * 1000);
        }
        return PlayerBase::getProgress();
    }

    void seekTo(float fProgress) override {
        fProgress = MAX(float(0), MIN(fProgress, float(1.0)));
        seekToMilliSecond((uint32_t)(fProgress * getDuration() * 1000));
    }

    void seekTo(uint32_t seekPos) override {
        uint32_t pos = MAX(float(0), MIN(seekPos, getDuration())) * 1000;
        seekToMilliSecond(pos);
    }

    float getDuration() const override {
        return _demuxer ? _demuxer->getDuration() : 0;
    }

    std::vector<Track::Ptr> getTracks(bool ready = true) const override {
        return _demuxer ? _demuxer->getTracks(ready) : Super::getTracks(ready);
    }

private:
    //派生类回调函数
    bool onCheckMeta(const AMFValue &val) override {
        //无metadata或metadata中无track信息时，需要从数据包中获取track
        _wait_track_ready = (*this)[Client::kWaitTrackReady].as<bool>() || RtmpDemuxer::trackCount(val) == 0;
        onCheckMeta_l(val);
        return true;
    }

    void onMediaData(RtmpPacket::Ptr chunkData) override {
        if (!_demuxer) {
            //有些rtmp流没metadata
            onCheckMeta_l(TitleMeta().getMetadata());
        }
        _demuxer->inputRtmp(chunkData);
        if (_rtmp_src) {
            _rtmp_src->onWrite(std::move(chunkData));
        }
    }

    void onPlayResult(const toolkit::SockException &ex) override {
        if (!_wait_track_ready || ex) {
            Super::onPlayResult(ex);
            return;
        }
    }

    bool addTrack(const Track::Ptr &track) override { return true; }

    void addTrackCompleted() override {
        if (_wait_track_ready) {
            Super::onPlayResult(toolkit::SockException(toolkit::Err_success, "play success"));
        }
    }

private:
    void onCheckMeta_l(const AMFValue &val) {
        _rtmp_src = std::dynamic_pointer_cast<RtmpMediaSource>(_media_src);
        if (_rtmp_src) {
            _rtmp_src->setMetaData(val);
        }
        if(_demuxer){
            return;
        }
        _demuxer = std::make_shared<RtmpDemuxer>();
        //TraceL<<" _wait_track_ready "<<_wait_track_ready;
        _demuxer->setTrackListener(this, _wait_track_ready);
        _demuxer->loadMetaData(val);
    }

private:
    bool _wait_track_ready = true;
    RtmpDemuxer::Ptr _demuxer;
    RtmpMediaSource::Ptr _rtmp_src;
};


} /* namespace mediakit */

#endif /* SRC_RTMP_RTMPPLAYERIMP_H_ */
