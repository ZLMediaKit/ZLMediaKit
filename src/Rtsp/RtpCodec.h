/*
 * Copyright (c) 2016 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/xia-chu/ZLMediaKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#ifndef ZLMEDIAKIT_RTPCODEC_H
#define ZLMEDIAKIT_RTPCODEC_H

#include <memory>
#include "Util/RingBuffer.h"
#include "Player/PlayerBase.h"

namespace mediakit {

class RtpRing {
public:
    using Ptr = std::shared_ptr<RtpRing>;
    using RingType = toolkit::RingBuffer<RtpPacket::Ptr>;

    RtpRing() = default;
    virtual ~RtpRing() = default;

    /**
     * 获取rtp环形缓存
     * @return
     */
    virtual RingType::Ptr getRtpRing() const {
        return _ring;
    }

    /**
     * 设置rtp环形缓存
     * @param ring
     */
    virtual void setRtpRing(const RingType::Ptr &ring) {
        _ring = ring;
    }

    /**
     * 输入rtp包
     * @param rtp rtp包
     * @param key_pos 是否为关键帧第一个rtp包
     * @return 是否为关键帧第一个rtp包
     */
    virtual bool inputRtp(const RtpPacket::Ptr &rtp, bool key_pos) {
        if (_ring) {
            _ring->write(rtp, key_pos);
        }
        return key_pos;
    }

protected:
    RingType::Ptr _ring;
};

class RtpInfo{
public:
    using Ptr = std::shared_ptr<RtpInfo>;

    RtpInfo(uint32_t ssrc, size_t mtu_size, uint32_t sample_rate, uint8_t pt, uint8_t interleaved) {
        if (ssrc == 0) {
            ssrc = ((uint64_t) this) & 0xFFFFFFFF;
        }
        _pt = pt;
        _ssrc = ssrc;
        _mtu_size = mtu_size;
        _sample_rate = sample_rate;
        _interleaved = interleaved;
    }

    virtual ~RtpInfo() {}

    //返回rtp负载最大长度
    size_t getMaxSize() const {
        return _mtu_size - RtpPacket::kRtpHeaderSize;
    }

    uint32_t getSsrc() const {
        return _ssrc;
    }

    RtpPacket::Ptr makeRtp(TrackType type,const void *data, size_t len, bool mark, uint64_t stamp);

private:
    uint8_t _pt;
    uint8_t _interleaved;
    uint16_t _seq = 0;
    uint32_t _ssrc;
    uint32_t _sample_rate;
    size_t _mtu_size;
};

class RtpCodec : public RtpRing, public FrameDispatcher, public CodecInfo {
public:
    using Ptr = std::shared_ptr<RtpCodec>;

    RtpCodec() = default;
    ~RtpCodec() override = default;
};

}//namespace mediakit




#endif //ZLMEDIAKIT_RTPCODEC_H
