/*
 * Copyright (c) 2016 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/xiongziliang/ZLMediaKit).
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
using namespace toolkit;

namespace mediakit{

class RtpRing{
public:
    typedef std::shared_ptr<RtpRing> Ptr;
    typedef RingBuffer<RtpPacket::Ptr> RingType;

    RtpRing(){}
    virtual ~RtpRing(){}

    /**
     * 获取rtp环形缓存
     * @return
     */
    virtual RingType::Ptr getRtpRing() const {
        return _rtpRing;
    }

    /**
     * 设置rtp环形缓存
     * @param ring
     */
    virtual void setRtpRing(const RingType::Ptr &ring){
        _rtpRing = ring;
    }

    /**
     * 输入rtp包
     * @param rtp rtp包
     * @param key_pos 是否为关键帧第一个rtp包
     * @return 是否为关键帧第一个rtp包
     */
    virtual bool inputRtp(const RtpPacket::Ptr &rtp, bool key_pos){
        if(_rtpRing){
            _rtpRing->write(rtp,key_pos);
        }
        return key_pos;
    }
protected:
    RingType::Ptr _rtpRing;
};


class RtpInfo : public  ResourcePoolHelper<RtpPacket>{
public:
    typedef std::shared_ptr<RtpInfo> Ptr;

    RtpInfo(uint32_t ui32Ssrc,
            uint32_t ui32MtuSize,
            uint32_t ui32SampleRate,
            uint8_t ui8PayloadType,
            uint8_t ui8Interleaved) {
        if(ui32Ssrc == 0){
            ui32Ssrc = ((uint64_t)this) & 0xFFFFFFFF;
        }
        _ui32Ssrc = ui32Ssrc;
        _ui32SampleRate = ui32SampleRate;
        _ui32MtuSize = ui32MtuSize;
        _ui8PayloadType = ui8PayloadType;
        _ui8Interleaved = ui8Interleaved;
    }

    virtual ~RtpInfo(){}

    int getInterleaved() const {
        return _ui8Interleaved;
    }

    int getPayloadType() const {
        return _ui8PayloadType;
    }

    int getSampleRate() const {
        return _ui32SampleRate;
    }

    uint32_t getSsrc() const {
        return _ui32Ssrc;
    }

    uint16_t getSeqence() const {
        return _ui16Sequence;
    }
    uint32_t getTimestamp() const {
        return _ui32TimeStamp;
    }
    uint32_t getMtuSize() const {
        return _ui32MtuSize;
    }
    RtpPacket::Ptr makeRtp(TrackType type,const void *pData, unsigned int uiLen, bool bMark, uint32_t uiStamp);
protected:
    uint32_t _ui32Ssrc;
    uint32_t _ui32SampleRate;
    uint32_t _ui32MtuSize;
    uint8_t _ui8PayloadType;
    uint8_t _ui8Interleaved;
    uint16_t _ui16Sequence = 0;
    uint32_t _ui32TimeStamp = 0;
};

class RtpCodec : public RtpRing, public FrameDispatcher , public CodecInfo{
public:
    typedef std::shared_ptr<RtpCodec> Ptr;
    RtpCodec(){}
    virtual ~RtpCodec(){}
};

}//namespace mediakit




#endif //ZLMEDIAKIT_RTPCODEC_H
