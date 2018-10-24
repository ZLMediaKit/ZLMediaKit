//
// Created by xzl on 2018/10/24.
//

#ifndef ZLMEDIAKIT_RTMPCODEC_H
#define ZLMEDIAKIT_RTMPCODEC_H

#include "Rtmp/Rtmp.h"
#include "Player/Frame.h"
#include "Util/RingBuffer.h"

using namespace ZL::Util;

namespace ZL{
namespace Rtmp {


class RtmpRingInterface {
public:
    typedef RingBuffer<RtmpPacket::Ptr> RingType;
    typedef std::shared_ptr<RtmpRingInterface> Ptr;

    RtmpRingInterface(){}
    virtual ~RtmpRingInterface(){}

    /**
     * 获取rtmp环形缓存
     * @return
     */
    virtual RingType::Ptr getRtmpRing() const = 0;

    /**
     * 设置rtmp环形缓存
     * @param ring
     */
    virtual void setRtmpRing(const RingType::Ptr &ring) = 0;

    /**
     * 输入rtmp包
     * @param rtmp rtmp包
     * @param key_pos 是否为关键帧
     * @return 是否为关键帧
     */
    virtual bool inputRtmp(const RtmpPacket::Ptr &rtmp, bool key_pos) = 0;
};

class RtmpRing : public RtmpRingInterface {
public:
    typedef std::shared_ptr<RtmpRing> Ptr;

    RtmpRing(){
        _rtmpRing = std::make_shared<RingType>();
    }
    virtual ~RtmpRing(){}

    RingType::Ptr getRtmpRing() const override {
        return _rtmpRing;
    }

    void setRtmpRing(const RingType::Ptr &ring) override {
        _rtmpRing = ring;
    }

    bool inputRtmp(const RtmpPacket::Ptr &rtmp, bool key_pos) override{
        _rtmpRing->write(rtmp,key_pos);
        return key_pos;
    }
protected:
    RingType::Ptr _rtmpRing;
};


class RtmpCodec : public RtmpRing, public FrameRingInterfaceDelegate , public CodecInfo{
public:
    typedef std::shared_ptr<RtmpCodec> Ptr;
    RtmpCodec(){}
    virtual ~RtmpCodec(){}
};




}//namespace Rtmp
}//namespace ZL

#endif //ZLMEDIAKIT_RTMPCODEC_H
