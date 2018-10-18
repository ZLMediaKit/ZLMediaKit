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

class RtpCodec{
public:
    typedef std::shared_ptr<RtpCodec> Ptr;
    typedef RingBuffer<Frame::Ptr> FrameRing;
    typedef RingBuffer<RtpPacket::Ptr> RtpRing;

    RtpCodec(){
        _frameRing = std::make_shared<FrameRing>();
        _rtpRing = std::make_shared<RtpRing>();
    }
    virtual ~RtpCodec(){}

    FrameRing::Ptr &getFrameRing() {
        return _frameRing;
    }
    RtpRing::Ptr &getRtpRing() {
        return _rtpRing;
    }

    virtual void inputFame(const Frame::Ptr &frame,bool key_pos){
        _frameRing->write(frame,key_pos);
    }
    virtual void inputRtp(const RtpPacket::Ptr &rtp, bool key_pos){
        _rtpRing->write(rtp,key_pos);
    }
private:
    FrameRing::Ptr _frameRing;
    RtpRing::Ptr _rtpRing;
};


class RtpEncoder : public RtpCodec{
public:
    typedef std::shared_ptr<RtpEncoder> Ptr;

    RtpEncoder(uint32_t ui32Ssrc,
               uint32_t ui32MtuSize,
               uint32_t ui32SampleRate,
               uint8_t ui8PlayloadType,
               uint8_t ui8Interleaved) {
        m_ui32Ssrc = ui32Ssrc;
        m_ui32SampleRate = ui32SampleRate;
        m_ui32MtuSize = ui32MtuSize;
        m_ui8PlayloadType = ui8PlayloadType;
        m_ui8Interleaved = ui8Interleaved;
        m_rtpPool.setSize(32);
    }

    ~RtpEncoder(){}

    int getInterleaved() const {
        return m_ui8Interleaved;
    }

    int getPlayloadType() const {
        return m_ui8PlayloadType;
    }

    int getSampleRate() const {
        return m_ui32SampleRate;
    }

    uint32_t getSsrc() const {
        return m_ui32Ssrc;
    }

    uint16_t getSeqence() const {
        return m_ui16Sequence;
    }
    uint32_t getTimestamp() const {
        return m_ui32TimeStamp;
    }
    uint32_t getMtuSize() const {
        return m_ui32MtuSize;
    }
protected:
    RtpPacket::Ptr obtainRtp(){
        return m_rtpPool.obtain();
    }
protected:
    uint32_t m_ui32Ssrc;
    uint32_t m_ui32SampleRate;
    uint32_t m_ui32MtuSize;
    uint8_t m_ui8PlayloadType;
    uint8_t m_ui8Interleaved;
    uint16_t m_ui16Sequence = 0;
    uint32_t m_ui32TimeStamp = 0;
    ResourcePool<RtpPacket> m_rtpPool;
};






#endif //ZLMEDIAKIT_RTPCODEC_H
