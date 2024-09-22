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
     * @param codec 编码类型
     * @param channels 通道数
     * Constructor
     * @param codec Encoding type
     * @param channels Number of channels
     
     * [AUTO-TRANSLATED:dbbd593e]
     */
    G711RtpEncoder(CodecId codec, uint32_t channels);

    /**
     * 输入帧数据并编码成rtp
     * Input frame data and encode it into rtp
     
     
     * [AUTO-TRANSLATED:02bc9009]
     */
    bool inputFrame(const Frame::Ptr &frame) override;

    void setOpt(int opt, const toolkit::Any &param) override;

private:
    uint32_t _channels = 1;
    uint32_t _pkt_dur_ms = 20;
    FrameImp::Ptr _cache_frame;
};

}//namespace mediakit
#endif //ZLMEDIAKIT_G711RTP_H
