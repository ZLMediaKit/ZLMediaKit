/*
 * Copyright (c) 2016-present The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/ZLMediaKit/ZLMediaKit).
 *
 * Use of this source code is governed by MIT-like license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#ifndef ZLMEDIAKIT_VP9RTPCODEC_H
#define ZLMEDIAKIT_VP9RTPCODEC_H

#include "VP9.h"
// for DtsGenerator
#include "Common/Stamp.h"
#include "Rtsp/RtpCodec.h"

namespace mediakit {

/**
 * VP9 rtp解码类
 * 将 VP9 over rtsp-rtp 解复用出 VP9Frame
 */
class VP9RtpDecoder : public RtpCodec {
public:
    using Ptr = std::shared_ptr<VP9RtpDecoder>;

    VP9RtpDecoder();

    /**
     * 输入VP9 rtp包
     * @param rtp rtp包
     * @param key_pos 此参数忽略之
     */
    bool inputRtp(const RtpPacket::Ptr &rtp, bool key_pos = true) override;

private:
    bool decodeRtp(const RtpPacket::Ptr &rtp);
    void outputFrame(const RtpPacket::Ptr &rtp);
    void obtainFrame();
private:
    bool _gop_dropped = false;
    bool _frame_drop = true;
    uint16_t _last_seq = 0;
    VP9Frame::Ptr _frame;
    DtsGenerator _dts_generator;
};

/**
 * VP9 rtp打包类
 */
class VP9RtpEncoder : public RtpCodec {
public:
    using Ptr = std::shared_ptr<VP9RtpEncoder>;

    bool inputFrame(const Frame::Ptr &frame) override;
private:
    uint16_t _pic_id = 0;
};

}//namespace mediakit

#endif //ZLMEDIAKIT_VP9RTPCODEC_H
