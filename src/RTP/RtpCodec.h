//
// Created by xzl on 2018/10/18.
//

#ifndef ZLMEDIAKIT_RTPCODEC_H
#define ZLMEDIAKIT_RTPCODEC_H

#include <memory>
#include "Util/RingBuffer.h"
#include "Rtsp/Rtsp.h"
#include "Player/PlayerBase.h"

using namespace std;
using namespace ZL::Util;
using namespace ZL::Player;

class RtpPacket : public CodecInfo {
public:
    typedef std::shared_ptr<RtpPacket> Ptr;

    TrackType getTrackType() const  {
        return type;
    }

    CodecId getCodecId() const {
        return CodecInvalid;
    }
public:
    uint8_t interleaved;
    uint8_t PT;
    bool mark;
    uint32_t length;
    uint32_t timeStamp;
    uint16_t sequence;
    uint32_t ssrc;
    uint8_t payload[1560];
    uint8_t offset;
    TrackType type;
};

class RtpRingInterface {
public:
    typedef RingBuffer<RtpPacket::Ptr> RingType;
    typedef std::shared_ptr<RtpRingInterface> Ptr;

    RtpRingInterface(){}
    virtual ~RtpRingInterface(){}

    /**
     * 获取rtp环形缓存
     * @return
     */
    virtual RingType::Ptr getRtpRing() const = 0;

    /**
     * 设置rtp环形缓存
     * @param ring
     */
    virtual void setRtpRing(const RingType::Ptr &ring) = 0;

    /**
     * 输入rtp包
     * @param rtp rtp包
     * @param key_pos 是否为关键帧第一个rtp包
     * @return 是否为关键帧第一个rtp包
     */
    virtual bool inputRtp(const RtpPacket::Ptr &rtp, bool key_pos) = 0;
};

class RtpRing : public RtpRingInterface {
public:
    typedef std::shared_ptr<RtpRing> Ptr;

    RtpRing(){
        _rtpRing = std::make_shared<RingType>();
    }
    virtual ~RtpRing(){}

    RingType::Ptr getRtpRing() const override {
        return _rtpRing;
    }

    void setRtpRing(const RingType::Ptr &ring) override {
        _rtpRing = ring;
    }

    bool inputRtp(const RtpPacket::Ptr &rtp, bool key_pos) override{
        _rtpRing->write(rtp,key_pos);
        return key_pos;
    }
protected:
    RingType::Ptr _rtpRing;
};


class RtpInfo{
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
protected:
    uint32_t _ui32Ssrc;
    uint32_t _ui32SampleRate;
    uint32_t _ui32MtuSize;
    uint8_t _ui8PlayloadType;
    uint8_t _ui8Interleaved;
    uint16_t _ui16Sequence = 0;
    uint32_t _ui32TimeStamp = 0;
};

class RtpCodec : public RtpRing, public FrameRingInterfaceDelegate , public CodecInfo ,  public ResourcePoolHelper<RtpPacket>{
public:
    typedef std::shared_ptr<RtpCodec> Ptr;
    RtpCodec(){}
    virtual ~RtpCodec(){}
};






#endif //ZLMEDIAKIT_RTPCODEC_H
