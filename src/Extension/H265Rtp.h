/*
 * Copyright (c) 2016 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/xiongziliang/ZLMediaKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#ifndef ZLMEDIAKIT_H265RTPCODEC_H
#define ZLMEDIAKIT_H265RTPCODEC_H

#include "Rtsp/RtpCodec.h"
#include "Util/ResourcePool.h"
#include "Extension/H265.h"
#include "Common/Stamp.h"

using namespace toolkit;

namespace mediakit{

/**
 * h265 rtp解码类
 * 将 h265 over rtsp-rtp 解复用出 h265-Frame
 * 《草案（H265-over-RTP）draft-ietf-payload-rtp-h265-07.pdf》
 */
class H265RtpDecoder : public RtpCodec , public ResourcePoolHelper<H265Frame> {
public:
    typedef std::shared_ptr<H265RtpDecoder> Ptr;

    H265RtpDecoder();
    ~H265RtpDecoder() {}

    /**
     * 输入265 rtp包
     * @param rtp rtp包
     * @param key_pos 此参数忽略之
     */
    bool inputRtp(const RtpPacket::Ptr &rtp, bool key_pos = true) override;

    CodecId getCodecId() const override{
        return CodecH265;
    }
private:
    bool decodeRtp(const RtpPacket::Ptr &rtp);
    void onGetH265(const H265Frame::Ptr &frame);
    H265Frame::Ptr obtainFrame();
private:
    H265Frame::Ptr _h265frame;
    DtsGenerator _dts_generator;
    int _lastSeq = 0;
};

/**
 * 265 rtp打包类
 */
class H265RtpEncoder : public H265RtpDecoder ,public RtpInfo{
public:
    typedef std::shared_ptr<H265RtpEncoder> Ptr;

    /**
     * @param ui32Ssrc ssrc
     * @param ui32MtuSize mtu大小
     * @param ui32SampleRate 采样率，强制为90000
     * @param ui8PayloadType pt类型
     * @param ui8Interleaved rtsp interleaved
     */
    H265RtpEncoder(uint32_t ui32Ssrc,
                   uint32_t ui32MtuSize = 1400,
                   uint32_t ui32SampleRate = 90000,
                   uint8_t ui8PayloadType = 96,
                   uint8_t ui8Interleaved = TrackVideo * 2);
    ~H265RtpEncoder() {}

    /**
     * 输入265帧
     * @param frame 帧数据，必须
     */
    void inputFrame(const Frame::Ptr &frame) override;
private:
    void makeH265Rtp(int nal_type,const void *pData, unsigned int uiLen, bool bMark, bool first_packet,uint32_t uiStamp);
};

}//namespace mediakit{

#endif //ZLMEDIAKIT_H265RTPCODEC_H
