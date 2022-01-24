/*
 * Copyright (c) 2016 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/xia-chu/ZLMediaKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#ifndef ZLMEDIAKIT_G711RTP_H
#define ZLMEDIAKIT_G711RTP_H

#include "Frame.h"
#include "CommonRtp.h"
#include "Rtsp/RtpCodec.h"

namespace mediakit {

/**
 * G711 rtp编码类
 */
class G711RtpEncoder : public CommonRtpDecoder, public RtpInfo {
public:
    using Ptr = std::shared_ptr<G711RtpEncoder>;

    ~G711RtpEncoder() override = default;

    /**
     * 构造函数
     * @param codec 编码类型
     * @param ssrc ssrc
     * @param mtu_size mtu 大小
     * @param sample_rate 采样率
     * @param payload_type pt类型
     * @param interleaved rtsp interleaved 值
     */
    G711RtpEncoder(CodecId codec, uint32_t ssrc, uint32_t mtu_size, uint32_t sample_rate, uint8_t payload_type,
                   uint8_t interleaved, uint32_t channels);

    /**
     * 输入帧数据并编码成rtp
     */
    bool inputFrame(const Frame::Ptr &frame) override;

private:
    uint32_t _channels = 1;
    FrameImp::Ptr _cache_frame;
};

}//namespace mediakit
#endif //ZLMEDIAKIT_G711RTP_H
