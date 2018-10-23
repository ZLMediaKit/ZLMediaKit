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
        //禁用缓存
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
        m_ui32Ssrc = ui32Ssrc;
        m_ui32SampleRate = ui32SampleRate;
        m_ui32MtuSize = ui32MtuSize;
        m_ui8PlayloadType = ui8PlayloadType;
        m_ui8Interleaved = ui8Interleaved;
        m_rtpPool.setSize(32);
    }

    virtual ~RtpInfo(){}

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

class RtpCodec : public RtpRing, public FrameRingInterface , public CodecInfo{
public:
    typedef std::shared_ptr<RtpCodec> Ptr;
    RtpCodec(){}
    virtual ~RtpCodec(){}

    void setDelegate(const FrameRingInterface::Ptr &delegate){
        _delegate = delegate;
    }
    /**
     * 获取帧环形缓存
     * @return
     */
    FrameRingInterface::RingType::Ptr getFrameRing() const override {
        if(_delegate){
            return _delegate->getFrameRing();
        }
        return nullptr;
    }

    /**
     * 设置帧环形缓存
     * @param ring
     */
    void setFrameRing(const FrameRingInterface::RingType::Ptr &ring) override {
        if(_delegate){
            _delegate->setFrameRing(ring);
        }
    }

    /**
     * 写入帧数据
     * @param frame 帧
     */
    void inputFrame(const Frame::Ptr &frame) override{
        if(_delegate){
            _delegate->inputFrame(frame);
        }
    }

    /**
     * 根据CodecId生成Rtp打包器
     * @param codecId
     * @param ui32Ssrc
     * @param ui32MtuSize
     * @param ui32SampleRate
     * @param ui8PlayloadType
     * @param ui8Interleaved
     * @return
     */
    static Ptr getRtpEncoderById(CodecId codecId,
                                 uint32_t ui32Ssrc,
                                 uint32_t ui32MtuSize,
                                 uint32_t ui32SampleRate,
                                 uint8_t ui8PlayloadType,
                                 uint8_t ui8Interleaved);

    /**
     * 根据CodecId生成Rtp解包器
     * @param codecId
     * @param ui32SampleRate
     * @return
     */
    static Ptr getRtpDecoderById(CodecId codecId,uint32_t ui32SampleRate);

private:
    FrameRingInterface::Ptr _delegate;
};






#endif //ZLMEDIAKIT_RTPCODEC_H
