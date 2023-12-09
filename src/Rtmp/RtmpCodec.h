/*
 * Copyright (c) 2016-present The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/ZLMediaKit/ZLMediaKit).
 *
 * Use of this source code is governed by MIT-like license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#ifndef ZLMEDIAKIT_RTMPCODEC_H
#define ZLMEDIAKIT_RTMPCODEC_H

#include "Rtmp/Rtmp.h"
#include "Extension/Frame.h"
#include "Util/RingBuffer.h"

namespace mediakit{

class RtmpRing {
public:
    using Ptr = std::shared_ptr<RtmpRing>;
    using RingType = toolkit::RingBuffer<RtmpPacket::Ptr>;

    virtual ~RtmpRing() = default;

    /**
     * 设置rtmp环形缓存
     */
    void setRtmpRing(const RingType::Ptr &ring) {
        _ring = ring;
    }

    /**
     * 输入rtmp包
     * @param rtmp rtmp包
     */
    virtual void inputRtmp(const RtmpPacket::Ptr &rtmp) {
        if (_ring) {
            _ring->write(rtmp, rtmp->isVideoKeyFrame());
        }
    }

protected:
    RingType::Ptr _ring;
};

class RtmpCodec : public RtmpRing, public FrameWriterInterface {
public:
    using Ptr = std::shared_ptr<RtmpCodec>;
    RtmpCodec(Track::Ptr track) { _track = std::move(track); }

    virtual void makeConfigPacket() {}

    bool inputFrame(const Frame::Ptr &frame) override { return _track->inputFrame(frame); }

    const Track::Ptr &getTrack() const { return _track; }

private:
    Track::Ptr _track;
};


}//namespace mediakit
#endif //ZLMEDIAKIT_RTMPCODEC_H
