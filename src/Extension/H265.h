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


#ifndef ZLMEDIAKIT_H265_H
#define ZLMEDIAKIT_H265_H

#include "Frame.h"
#include "Track.h"

namespace mediakit{

/**
* 265帧类
*/
class H265Frame : public Frame {
public:
    typedef std::shared_ptr<H265Frame> Ptr;

    char *data() const override{
        return (char *)buffer.data();
    }
    uint32_t size() const override {
        return buffer.size();
    }
    uint32_t stamp() const override {
        return timeStamp;
    }
    uint32_t prefixSize() const override{
        return iPrefixSize;
    }

    TrackType getTrackType() const override{
        return TrackVideo;
    }

    CodecId getCodecId() const override{
        return CodecH265;
    }

    bool keyFrame() const override {
        return type == 5;
    }
public:
    uint16_t sequence;
    uint32_t timeStamp;
    unsigned char type;
    string buffer;
    uint32_t iPrefixSize = 4;
};


class H265FrameNoCopyAble : public FrameNoCopyAble {
public:
    typedef std::shared_ptr<H265FrameNoCopyAble> Ptr;

    H265FrameNoCopyAble(char *ptr,uint32_t size,uint32_t stamp,int prefixeSize = 4){
        buffer_ptr = ptr;
        buffer_size = size;
        timeStamp = stamp;
        iPrefixSize = prefixeSize;
    }

    TrackType getTrackType() const override{
        return TrackVideo;
    }

    CodecId getCodecId() const override{
        return CodecH265;
    }

    bool keyFrame() const override {
        return (buffer_ptr[iPrefixSize] & 0x1F) == 5;
    }
};


}//namespace mediakit


#endif //ZLMEDIAKIT_H265_H
