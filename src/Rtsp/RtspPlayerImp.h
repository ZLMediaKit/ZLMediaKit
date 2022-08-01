/*
 * Copyright (c) 2016 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/xia-chu/ZLMediaKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#ifndef SRC_RTP_RTPPARSERTESTER_H_
#define SRC_RTP_RTPPARSERTESTER_H_

#include <memory>
#include <algorithm>
#include <functional>
#include "Common/config.h"
#include "RtspPlayer.h"
#include "RtspDemuxer.h"
#include "Poller/Timer.h"
#include "Util/TimeTicker.h"

namespace mediakit {

class RtspPlayerImp : public PlayerImp<RtspPlayer, PlayerBase> ,private TrackListener {
public:
    using Ptr = std::shared_ptr<RtspPlayerImp>;
    using Super = PlayerImp<RtspPlayer, PlayerBase>;

    RtspPlayerImp(const toolkit::EventPoller::Ptr &poller) : Super(poller) {}

    ~RtspPlayerImp() override {
        DebugL << std::endl;
    }

    float getProgress() const override {
        if (getDuration() > 0) {
            return getProgressMilliSecond() / (getDuration() * 1000);
        }
        return PlayerBase::getProgress();
    }

    uint32_t getProgressPos() const override {
        if (getDuration() > 0) {
            return getProgressMilliSecond();
        }
        return PlayerBase::getProgressPos();
    }

    void seekTo(float fProgress) override {
        fProgress = MAX(float(0), MIN(fProgress, float(1.0)));
        seekToMilliSecond((uint32_t) (fProgress * getDuration() * 1000));
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
    bool onCheckSDP(const std::string &sdp) override {
        _rtsp_media_src = std::dynamic_pointer_cast<RtspMediaSource>(_media_src);
        if (_rtsp_media_src) {
            _rtsp_media_src->setSdp(sdp);
        }
        _demuxer = std::make_shared<RtspDemuxer>();
        _demuxer->setTrackListener(this, (*this)[Client::kWaitTrackReady].as<bool>());
        _demuxer->loadSdp(sdp);
        return true;
    }

    void onRecvRTP(RtpPacket::Ptr rtp, const SdpTrack::Ptr &track) override {
        //rtp解复用时可以判断是否为关键帧起始位置
        auto key_pos = _demuxer->inputRtp(rtp);
        if (_rtsp_media_src) {
            _rtsp_media_src->onWrite(std::move(rtp), key_pos);
        }
    }

    void onPlayResult(const toolkit::SockException &ex) override {
        if (!(*this)[Client::kWaitTrackReady].as<bool>() || ex) {
            Super::onPlayResult(ex);
            return;
        }
    }

    bool addTrack(const Track::Ptr &track) override { return true; }

    void addTrackCompleted() override {
        if ((*this)[Client::kWaitTrackReady].as<bool>()) {
            Super::onPlayResult(toolkit::SockException(toolkit::Err_success, "play success"));
        }
    }

private:
    RtspDemuxer::Ptr _demuxer;
    RtspMediaSource::Ptr _rtsp_media_src;
};

} /* namespace mediakit */

#endif /* SRC_RTP_RTPPARSERTESTER_H_ */
