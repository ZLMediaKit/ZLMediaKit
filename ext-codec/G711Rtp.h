/*
 * Copyright (c) 2016-present The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/ZLMediaKit/ZLMediaKit).
 *
 * Use of this source code is governed by MIT-like license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#ifndef ZLMEDIAKIT_G711RTP_H
#define ZLMEDIAKIT_G711RTP_H

#include "Rtsp/RtpCodec.h"
#include "Extension/Frame.h"
#include "Extension/CommonRtp.h"

namespace mediakit {

/**
 * G711 rtp编码类
 * G711 rtp encoding class
 
 * [AUTO-TRANSLATED:92aa6cf3]
 */
class G711RtpEncoder : public RtpCodec {
public:
    using Ptr = std::shared_ptr<G711RtpEncoder>;

    /**
     * 构造函数
     * @param sample_rate 音频采样率
     * @param channels 通道数
     * @param sample_bit 音频采样位数
     * Constructor
     * @param sample_rate audio sample rate
     * @param channels Number of channels
     * @param sample_bit audio sample bits

     * [AUTO-TRANSLATED:dbbd593e]
     */
    G711RtpEncoder(int sample_rate = 8000, int channels = 1, int sample_bit = 16);

    /**
     * 输入帧数据并编码成rtp
     * Input frame data and encode it into rtp
     
     
     * [AUTO-TRANSLATED:02bc9009]
     */
    bool inputFrame(const Frame::Ptr &frame) override;

    void setOpt(int opt, const toolkit::Any &param) override;

private:
    int _channels;
    int _sample_rate;
    int _sample_bit;

    uint32_t _pkt_dur_ms = 20;
    uint32_t _pkt_bytes = 0;
    toolkit::BufferLikeString _buffer;
};

}//namespace mediakit
#endif //ZLMEDIAKIT_G711RTP_H
