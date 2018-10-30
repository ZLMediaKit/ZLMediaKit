/*
 * MIT License
 *
 * Copyright (c) 2016 xiongziliang <771730766@qq.com>
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

#ifndef ZLMEDIAKIT_RTMPCODEC_H
#define ZLMEDIAKIT_RTMPCODEC_H

#include "Rtmp/Rtmp.h"
#include "Extension/Frame.h"
#include "Util/RingBuffer.h"
using namespace toolkit;

namespace mediakit{

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
    }
    virtual ~RtmpRing(){}

    RingType::Ptr getRtmpRing() const override {
        return _rtmpRing;
    }

    void setRtmpRing(const RingType::Ptr &ring) override {
        _rtmpRing = ring;
    }

    bool inputRtmp(const RtmpPacket::Ptr &rtmp, bool key_pos) override{
        if(_rtmpRing){
            _rtmpRing->write(rtmp,key_pos);
        }
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


}//namespace mediakit

#endif //ZLMEDIAKIT_RTMPCODEC_H
