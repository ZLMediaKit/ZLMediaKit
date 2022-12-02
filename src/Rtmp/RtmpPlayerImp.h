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
#include "RtmpPlayer.h"
#include "RtmpDemuxer.h"
#include "RtmpMediaSource.h"

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

    float getDuration() const override;

    std::vector<Track::Ptr> getTracks(bool ready = true) const override;

private:
    //派生类回调函数
    bool onCheckMeta(const AMFValue &val) override;

    void onMediaData(RtmpPacket::Ptr chunkData) override;

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
    void onCheckMeta_l(const AMFValue &val);

private:
    bool _wait_track_ready = true;
    RtmpDemuxer::Ptr _demuxer;
    RtmpMediaSource::Ptr _rtmp_src;
};


} /* namespace mediakit */

#endif /* SRC_RTMP_RTMPPLAYERIMP_H_ */
