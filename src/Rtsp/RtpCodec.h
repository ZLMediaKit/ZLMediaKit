/*
 * Copyright (c) 2016-present The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/ZLMediaKit/ZLMediaKit).
 *
 * Use of this source code is governed by MIT-like license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#ifndef ZLMEDIAKIT_RTPCODEC_H
#define ZLMEDIAKIT_RTPCODEC_H

#include <memory>
#include "Extension/Frame.h"
#include "Util/RingBuffer.h"
#include "Rtsp/Rtsp.h"

namespace mediakit {

class RtpRing {
public:
    using Ptr = std::shared_ptr<RtpRing>;
    using RingType = toolkit::RingBuffer<RtpPacket::Ptr>;

    virtual ~RtpRing() = default;

    /**
     * 设置rtp环形缓存
     * @param ring
     */
    void setRtpRing(RingType::Ptr ring) {
        _ring = std::move(ring);
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

class RtpInfo {
public:
    using Ptr = std::shared_ptr<RtpInfo>;

    RtpInfo(uint32_t ssrc, size_t mtu_size, uint32_t sample_rate, uint8_t pt, uint8_t interleaved, int track_index) {
        if (ssrc == 0) {
            ssrc = ((uint64_t) this) & 0xFFFFFFFF;
        }
        _pt = pt;
        _ssrc = ssrc;
        _mtu_size = mtu_size;
        _sample_rate = sample_rate;
        _interleaved = interleaved;
        _track_index = track_index;
    }

    //返回rtp负载最大长度
    size_t getMaxSize() const {
        return _mtu_size - RtpPacket::kRtpHeaderSize;
    }

    RtpPacket::Ptr makeRtp(TrackType type,const void *data, size_t len, bool mark, uint64_t stamp);

private:
    uint8_t _pt;
    uint8_t _interleaved;
    uint16_t _seq = 0;
    uint32_t _ssrc;
    uint32_t _sample_rate;
    int _track_index;
    size_t _mtu_size;
};

class RtpCodec : public RtpRing, public FrameDispatcher {
public:
    using Ptr = std::shared_ptr<RtpCodec>;

    void setRtpInfo(uint32_t ssrc, size_t mtu_size, uint32_t sample_rate, uint8_t pt, uint8_t interleaved = 0, int track_index = 0) {
        _rtp_info.reset(new RtpInfo(ssrc, mtu_size, sample_rate, pt, interleaved, track_index));
    }

    RtpInfo &getRtpInfo() { return *_rtp_info; }

    enum {
        RTP_ENCODER_PKT_DUR_MS = 1 // 主要应用于g711 rtp 打包器每个包的时间长度，option_value 为int*, option_len 为4
    };
    /**
     * @brief 设置rtp打包器与解包器的相关参数，主要应用与g711 rtp 打包器，使用方法类似setsockopt
     *
     * @param opt 设置的选项
     * @param param 设置的参数
     */
    virtual void setOpt(int opt, const toolkit::Any &param) {};

private:
    std::unique_ptr<RtpInfo> _rtp_info;
};

}//namespace mediakit


#endif //ZLMEDIAKIT_RTPCODEC_H
