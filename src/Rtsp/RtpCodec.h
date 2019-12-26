/*
 * MIT License
 *
 * Copyright (c) 2016-2019 xiongziliang <771730766@qq.com>
 *
 * This file is part of ZLMediaKit(https://github.com/xiongziliang/ZLMediaKit).
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
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
            uint8_t ui8PlayloadType,
            uint8_t ui8Interleaved) {
        if(ui32Ssrc == 0){
            ui32Ssrc = ((uint64_t)this) & 0xFFFFFFFF;
        }
        _ui32Ssrc = ui32Ssrc;
        _ui32SampleRate = ui32SampleRate;
        _ui32MtuSize = ui32MtuSize;
        _ui8PlayloadType = ui8PlayloadType;
        _ui8Interleaved = ui8Interleaved;
    }

    virtual ~RtpInfo(){}

    int getInterleaved() const {
        return _ui8Interleaved;
    }

    int getPlayloadType() const {
        return _ui8PlayloadType;
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
    uint8_t _ui8PlayloadType;
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
