/*
 * Copyright (c) 2016 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/xia-chu/ZLMediaKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#include "RtspMuxer.h"
#include "Extension/Factory.h"

using namespace std;
using namespace toolkit;

namespace mediakit {

void RtspMuxer::onRtp(RtpPacket::Ptr in, bool is_key) {
    if (_live) {
        if (_rtp_stamp[in->type] != in->getHeader()->stamp) {
            //rtp时间戳变化才计算ntp，节省cpu资源
            int64_t stamp_ms = in->getStamp() * uint64_t(1000) / in->sample_rate;
            int64_t stamp_ms_inc;
            //求rtp时间戳增量
            _stamp[in->type].revise(stamp_ms, stamp_ms, stamp_ms_inc, stamp_ms_inc);
            _rtp_stamp[in->type] = in->getHeader()->stamp;
            _ntp_stamp[in->type] = stamp_ms_inc + _ntp_stamp_start;
        }

        //rtp拦截入口，此处统一赋值ntp
        in->ntp_stamp = _ntp_stamp[in->type];
    } else {
        //点播情况下设置ntp时间戳为rtp时间戳加基准ntp时间戳
        in->ntp_stamp = _ntp_stamp_start + (in->getStamp() * uint64_t(1000) / in->sample_rate);
    }
    _rtpRing->write(std::move(in), is_key);
}

RtspMuxer::RtspMuxer(const TitleSdp::Ptr &title) {
    if (!title) {
        _sdp = std::make_shared<TitleSdp>()->getSdp();
    } else {
        _live = title->getDuration() == 0;
        _sdp = title->getSdp();
    }
    _rtpRing = std::make_shared<RtpRing::RingType>();
    _rtpInterceptor = std::make_shared<RtpRing::RingType>();
    _rtpInterceptor->setDelegate(std::make_shared<RingDelegateHelper>([this](RtpPacket::Ptr in, bool is_key) {
        onRtp(std::move(in), is_key);
    }));
    _ntp_stamp_start = getCurrentMillisecond(true);
}

bool RtspMuxer::addTrack(const Track::Ptr &track) {
    //根据track生成sdp
    Sdp::Ptr sdp = track->getSdp();
    if (!sdp) {
        return false;
    }

    auto &encoder = _encoder[track->getTrackType()];
    encoder = Factory::getRtpEncoderBySdp(sdp);
    if (!encoder) {
        return false;
    }

    //设置rtp输出环形缓存
    encoder->setRtpRing(_rtpInterceptor);

    //添加其sdp
    _sdp.append(sdp->getSdp());
    trySyncTrack();
    return true;
}

void RtspMuxer::trySyncTrack() {
    if (_encoder[TrackAudio] && _encoder[TrackVideo]) {
        //音频时间戳同步于视频，因为音频时间戳被修改后不影响播放
        _stamp[TrackAudio].syncTo(_stamp[TrackVideo]);
    }
}

bool RtspMuxer::inputFrame(const Frame::Ptr &frame) {
    auto &encoder = _encoder[frame->getTrackType()];
    return encoder ? encoder->inputFrame(frame) : false;
}

string RtspMuxer::getSdp() {
    return _sdp;
}

RtpRing::RingType::Ptr RtspMuxer::getRtpRing() const {
    return _rtpRing;
}

void RtspMuxer::resetTracks() {
    _sdp.clear();
    for (auto &encoder : _encoder) {
        encoder = nullptr;
    }
}


} /* namespace mediakit */