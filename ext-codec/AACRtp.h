/*
 * Copyright (c) 2016-present The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/ZLMediaKit/ZLMediaKit).
 *
 * Use of this source code is governed by MIT-like license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#ifndef ZLMEDIAKIT_AACRTPCODEC_H
#define ZLMEDIAKIT_AACRTPCODEC_H

#include "AAC.h"
#include "Rtsp/RtpCodec.h"

namespace mediakit {
/**
 * aac rtp转adts类
 */
class AACRtpDecoder : public RtpCodec {
public:
    using Ptr = std::shared_ptr<AACRtpDecoder>;

    AACRtpDecoder();

    /**
     * 输入rtp并解码
     * @param rtp rtp数据包
     * @param key_pos 此参数内部强制转换为false,请忽略之
     */
    bool inputRtp(const RtpPacket::Ptr &rtp, bool key_pos = false) override;

private:
    void obtainFrame();
    void flushData();

private:
    uint64_t _last_dts = 0;
    FrameImp::Ptr _frame;
};


/**
 * aac adts转rtp类
 */
class AACRtpEncoder : public RtpCodec {
public:
    using Ptr = std::shared_ptr<AACRtpEncoder>;

    /**
     * 输入aac 数据，必须带dats头
     * @param frame 带dats头的aac数据
     */
    bool inputFrame(const Frame::Ptr &frame) override;

private:
    void outputRtp(const char *data, size_t len, size_t total_len, bool mark, uint64_t stamp);

};

}//namespace mediakit

#endif //ZLMEDIAKIT_AACRTPCODEC_H
