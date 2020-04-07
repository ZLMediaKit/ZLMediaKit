/*
 * Copyright (c) 2016 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/xiongziliang/ZLMediaKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#ifndef ZLMEDIAKIT_RTMPCODEC_H
#define ZLMEDIAKIT_RTMPCODEC_H

#include "Rtmp/Rtmp.h"
#include "Extension/Frame.h"
#include "Util/RingBuffer.h"
using namespace toolkit;

namespace mediakit{

class RtmpRing{
public:
    typedef std::shared_ptr<RtmpRing> Ptr;
    typedef RingBuffer<RtmpPacket::Ptr> RingType;

    RtmpRing(){}
    virtual ~RtmpRing(){}

    /**
     * 获取rtmp环形缓存
     * @return
     */
    virtual RingType::Ptr getRtmpRing() const{
        return _rtmpRing;
    }

    /**
     * 设置rtmp环形缓存
     * @param ring
     */
    virtual void setRtmpRing(const RingType::Ptr &ring){
        _rtmpRing = ring;
    }

    /**
     * 输入rtmp包
     * @param rtmp rtmp包
     * @param key_pos 是否为关键帧
     * @return 是否为关键帧
     */
    virtual bool inputRtmp(const RtmpPacket::Ptr &rtmp, bool key_pos){
        if(_rtmpRing){
            _rtmpRing->write(rtmp,key_pos);
        }
        return key_pos;
    }
protected:
    RingType::Ptr _rtmpRing;
};


class RtmpCodec : public RtmpRing, public FrameDispatcher , public CodecInfo{
public:
    typedef std::shared_ptr<RtmpCodec> Ptr;
    RtmpCodec(){}
    virtual ~RtmpCodec(){}
    virtual void makeConfigPacket() {};
};


}//namespace mediakit

#endif //ZLMEDIAKIT_RTMPCODEC_H
