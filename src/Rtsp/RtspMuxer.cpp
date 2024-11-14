/*
 * Copyright (c) 2016-present The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/ZLMediaKit/ZLMediaKit).
 *
 * Use of this source code is governed by MIT-like license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#include "RtspMuxer.h"
#include "Common/config.h"
#include "Extension/Factory.h"

using namespace std;
using namespace toolkit;

namespace mediakit {

void RtspMuxer::onRtp(RtpPacket::Ptr in, bool is_key) {
    if (_live) {
        auto &ref = _tracks[in->track_index];
        if (ref.rtp_stamp != in->getHeader()->stamp) {
            // rtp时间戳变化才计算ntp，节省cpu资源  [AUTO-TRANSLATED:729d54f2]
            // Only calculate NTP when the RTP timestamp changes, saving CPU resources
            int64_t stamp_ms_inc;
            // 求rtp时间戳增量  [AUTO-TRANSLATED:f6ba022f]
            // Get the RTP timestamp increment
            ref.stamp.revise(in->ntp_stamp, in->ntp_stamp, stamp_ms_inc, stamp_ms_inc);
            ref.rtp_stamp = in->getHeader()->stamp;
            ref.ntp_stamp = stamp_ms_inc + _ntp_stamp_start;
        }

        // rtp拦截入口，此处统一赋值ntp  [AUTO-TRANSLATED:1412435a]
        // RTP interception entry, set NTP here uniformly
        in->ntp_stamp = ref.ntp_stamp;
    } else {
        // 点播情况下设置ntp时间戳为rtp时间戳加基准ntp时间戳  [AUTO-TRANSLATED:b9f77de4]
        // In on-demand scenarios, set the NTP timestamp to the RTP timestamp plus the base NTP timestamp
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
    if (_track_existed[track->getTrackType()]) {
        // rtsp不支持多个同类型track  [AUTO-TRANSLATED:87262d86]
        // RTSP does not support multiple tracks of the same type
        WarnL << "Already add a track kind of: " << track->getTrackTypeStr() << ", ignore track: " << track->getCodecName();
        return false;
    }

    auto &ref = _tracks[track->getIndex()];
    auto &encoder = ref.encoder;
    CHECK(!encoder);

    // payload type 96以后则为动态pt  [AUTO-TRANSLATED:812ac0a2]
    // Payload type 96 and above is dynamic PT
    Sdp::Ptr sdp = track->getSdp(96 + _index);
    if (!sdp) {
        WarnL << "Unsupported codec: " << track->getCodecName();
        return false;
    }

    encoder = Factory::getRtpEncoderByCodecId(track->getCodecId(), sdp->getPayloadType());
    if (!encoder) {
        return false;
    }

    // 标记已经存在该类型track  [AUTO-TRANSLATED:ed79ebb5]
    // Mark that a track of this type already exists
    _track_existed[track->getTrackType()] = true;

    {
        static atomic<uint32_t> s_ssrc(0);
        uint32_t ssrc = s_ssrc++;
        if (!ssrc) {
            // ssrc不能为0  [AUTO-TRANSLATED:312a1b47]
            // SSRC cannot be 0
            ssrc = s_ssrc++;
        }
        if (track->getTrackType() == TrackVideo) {
            // 视频的ssrc是偶数，方便调试  [AUTO-TRANSLATED:c22cd03f]
            // The video SSRC is even for debugging convenience
            ssrc = 2 * ssrc;
        } else {
            // 音频ssrc是奇数  [AUTO-TRANSLATED:50688636]
            // The audio SSRC is odd
            ssrc = 2 * ssrc + 1;
        }
        GET_CONFIG(uint32_t, audio_mtu, Rtp::kAudioMtuSize);
        GET_CONFIG(uint32_t, video_mtu, Rtp::kVideoMtuSize);
        auto mtu = track->getTrackType() == TrackVideo ? video_mtu : audio_mtu;
        encoder->setRtpInfo(ssrc, mtu, sdp->getSampleRate(), sdp->getPayloadType(), 2 * track->getTrackType(), track->getIndex());
    }

    // 设置rtp输出环形缓存  [AUTO-TRANSLATED:5ac7e24a]
    // Set the RTP output circular buffer
    encoder->setRtpRing(_rtpInterceptor);

    auto str = sdp->getSdp();
    str += "a=control:trackID=";
    str += std::to_string(_index);
    str += "\r\n";

    // 添加其sdp  [AUTO-TRANSLATED:80958925]
    // Add its SDP
    _sdp.append(str);
    trySyncTrack();

    // rtp的时间戳是pts，允许回退  [AUTO-TRANSLATED:f4a977fc]
    // The RTP timestamp is PTS, allowing rollback
    if (track->getTrackType() == TrackVideo) {
        ref.stamp.enableRollback(true);
    }
    ++_index;
    return true;
}

void RtspMuxer::trySyncTrack() {
    Stamp *first = nullptr;
    for (auto &pr : _tracks) {
        if (!first) {
            first = &pr.second.stamp;
        } else {
            pr.second.stamp.syncTo(*first);
        }
    }
}

bool RtspMuxer::inputFrame(const Frame::Ptr &frame) {
    auto &encoder = _tracks[frame->getIndex()].encoder;
    return encoder ? encoder->inputFrame(frame) : false;
}

void RtspMuxer::flush() {
    for (auto &pr : _tracks) {
        if (pr.second.encoder) {
            pr.second.encoder->flush();
        }
    }
}

string RtspMuxer::getSdp() {
    return _sdp;
}

RtpRing::RingType::Ptr RtspMuxer::getRtpRing() const {
    return _rtpRing;
}

void RtspMuxer::resetTracks() {
    _sdp.clear();
    _tracks.clear();
    CLEAR_ARR(_track_existed);
}

} /* namespace mediakit */