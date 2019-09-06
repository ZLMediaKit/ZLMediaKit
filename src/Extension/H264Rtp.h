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

#ifndef ZLMEDIAKIT_H264RTPCODEC_H
#define ZLMEDIAKIT_H264RTPCODEC_H

#include "Rtsp/RtpCodec.h"
#include "Util/ResourcePool.h"
#include "Extension/H264.h"
using namespace toolkit;

namespace mediakit{

/**
 * h264 rtp解码类
 * 将 h264 over rtsp-rtp 解复用出 h264-Frame
 * rfc3984
 */
class H264RtpDecoder : public RtpCodec , public ResourcePoolHelper<H264Frame> {
public:
    typedef std::shared_ptr<H264RtpDecoder> Ptr;

    H264RtpDecoder();
    ~H264RtpDecoder() {}

    /**
     * 输入264 rtp包
     * @param rtp rtp包
     * @param key_pos 此参数忽略之
     */
    bool inputRtp(const RtpPacket::Ptr &rtp, bool key_pos = true) override;

    TrackType getTrackType() const override{
        return TrackVideo;
    }

    CodecId getCodecId() const override{
        return CodecH264;
    }
private:
    bool decodeRtp(const RtpPacket::Ptr &rtp);
    void onGetH264(const H264Frame::Ptr &frame);
    H264Frame::Ptr obtainFrame();
private:
    H264Frame::Ptr _h264frame;
    int _lastSeq = 0;
};

/**
 * 264 rtp打包类
 */
class H264RtpEncoder : public H264RtpDecoder ,public RtpInfo{
public:
    typedef std::shared_ptr<H264RtpEncoder> Ptr;

    /**
     * @param ui32Ssrc ssrc
     * @param ui32MtuSize mtu大小
     * @param ui32SampleRate 采样率，强制为90000
     * @param ui8PlayloadType pt类型
     * @param ui8Interleaved rtsp interleaved
     */
    H264RtpEncoder(uint32_t ui32Ssrc,
                   uint32_t ui32MtuSize = 1400,
                   uint32_t ui32SampleRate = 90000,
                   uint8_t ui8PlayloadType = 96,
                   uint8_t ui8Interleaved = TrackVideo * 2);
    ~H264RtpEncoder() {}

    /**
     * 输入264帧
     * @param frame 帧数据，必须
     */
    void inputFrame(const Frame::Ptr &frame) override;
private:
    void makeH264Rtp(int nal_type,const void *pData, unsigned int uiLen, bool bMark,  bool first_packet, uint32_t uiStamp);
};

}//namespace mediakit{

#endif //ZLMEDIAKIT_H264RTPCODEC_H
