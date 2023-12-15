/*
 * Copyright (c) 2016-present The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/ZLMediaKit/ZLMediaKit).
 *
 * Use of this source code is governed by MIT-like license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#include "RtmpMuxer.h"
#include "Extension/Factory.h"

namespace mediakit {

RtmpMuxer::RtmpMuxer(const TitleMeta::Ptr &title) {
    if (!title) {
        _metadata = std::make_shared<TitleMeta>()->getMetadata();
    } else {
        _metadata = title->getMetadata();
    }
    _rtmp_ring = std::make_shared<RtmpRing::RingType>();
}

bool RtmpMuxer::addTrack(const Track::Ptr &track) {
    if (_track_existed[track->getTrackType()]) {
        // rtmp不支持多个同类型track
        WarnL << "Already add a track kind of: " << track->getTrackTypeStr() << ", ignore track: " << track->getCodecName();
        return false;
    }

    auto &encoder = _encoders[track->getIndex()];
    CHECK(!encoder);
    encoder = Factory::getRtmpEncoderByTrack(track);
    if (!encoder) {
        return false;
    }

    // 标记已经存在该类型track
    _track_existed[track->getTrackType()] = true;

    // 设置rtmp输出环形缓存
    encoder->setRtmpRing(_rtmp_ring);

    // 添加metadata
    Metadata::addTrack(_metadata, track);
    return true;
}

bool RtmpMuxer::inputFrame(const Frame::Ptr &frame) {
    auto &encoder = _encoders[frame->getIndex()];
    return encoder ? encoder->inputFrame(frame) : false;
}

void RtmpMuxer::flush() {
    for (auto &pr : _encoders) {
        if (pr.second) {
            pr.second->flush();
        }
    }
}

void RtmpMuxer::makeConfigPacket() {
    for (auto &pr : _encoders) {
        if (pr.second) {
            pr.second->makeConfigPacket();
        }
    }
}

const AMFValue &RtmpMuxer::getMetadata() const {
    return _metadata;
}

RtmpRing::RingType::Ptr RtmpMuxer::getRtmpRing() const {
    return _rtmp_ring;
}

void RtmpMuxer::resetTracks() {
    _metadata.clear();
    _encoders.clear();
    CLEAR_ARR(_track_existed);
}

} /* namespace mediakit */