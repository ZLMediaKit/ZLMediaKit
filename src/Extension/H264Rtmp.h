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

#ifndef ZLMEDIAKIT_H264RTMPCODEC_H
#define ZLMEDIAKIT_H264RTMPCODEC_H

#include "Rtmp/RtmpCodec.h"
#include "Extension/Track.h"
#include "Util/ResourcePool.h"
#include "Extension/H264.h"
using namespace toolkit;

namespace mediakit{
/**
 * h264 Rtmp解码类
 * 将 h264 over rtmp 解复用出 h264-Frame
 */
class H264RtmpDecoder : public RtmpCodec ,public ResourcePoolHelper<H264Frame> {
public:
    typedef std::shared_ptr<H264RtmpDecoder> Ptr;

    H264RtmpDecoder();
    ~H264RtmpDecoder() {}

    /**
     * 输入264 Rtmp包
     * @param rtmp Rtmp包
     * @param key_pos 此参数忽略之
     */
    bool inputRtmp(const RtmpPacket::Ptr &rtmp, bool key_pos = true) override;

    TrackType getTrackType() const override{
        return TrackVideo;
    }

    CodecId getCodecId() const override{
        return CodecH264;
    }
protected:
    bool decodeRtmp(const RtmpPacket::Ptr &Rtmp);
    void onGetH264(const char *pcData, int iLen, uint32_t dts,uint32_t pts);
    H264Frame::Ptr obtainFrame();
protected:
    H264Frame::Ptr _h264frame;
    string _sps;
    string _pps;
};

/**
 * 264 Rtmp打包类
 */
class H264RtmpEncoder : public H264RtmpDecoder, public ResourcePoolHelper<RtmpPacket> {
public:
    typedef std::shared_ptr<H264RtmpEncoder> Ptr;

    /**
     * 构造函数，track可以为空，此时则在inputFrame时输入sps pps
     * 如果track不为空且包含sps pps信息，
     * 那么inputFrame时可以不输入sps pps
     * @param track
     */
    H264RtmpEncoder(const Track::Ptr &track);
    ~H264RtmpEncoder() {}

    /**
     * 输入264帧，可以不带sps pps
     * @param frame 帧数据
     */
    void inputFrame(const Frame::Ptr &frame) override;

    /**
     * 生成config包
     */
    void makeConfigPacket() override;
private:
    void makeVideoConfigPkt();
private:
    H264Track::Ptr _track;
    bool _gotSpsPps = false;
    RtmpPacket::Ptr _lastPacket;
};

}//namespace mediakit

#endif //ZLMEDIAKIT_H264RTMPCODEC_H
