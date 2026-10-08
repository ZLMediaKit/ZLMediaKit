/*
 * Copyright (c) 2016-present The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/ZLMediaKit/ZLMediaKit).
 *
 * Use of this source code is governed by MIT-like license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#ifndef ZLMEDIAKIT_AV1RTP_H
#define ZLMEDIAKIT_AV1RTP_H

#include "AV1.h"
#include "Rtsp/RtpCodec.h"

namespace mediakit {

class AV1RtpDecoder : public RtpCodec {
public:
    using Ptr = std::shared_ptr<AV1RtpDecoder>;

    /**
     * 输入AV rtp包
     * @param rtp rtp包
     * @param key_pos 此参数忽略之
     */
    bool inputRtp(const RtpPacket::Ptr &rtp, bool key_pos = true) override;

private:
    void flushFrame();

private:
    bool _gop_dropped = false;
    uint16_t _last_seq = 0;
    uint32_t _rtp_stamp = 0;
    AV1Frame::Ptr _frame;
};

class AV1RtpEncoder : public AV1RtpDecoder {
public:
    using Ptr = std::shared_ptr<AV1RtpEncoder>;

    /**
     * 输入AV帧
     * @param frame 帧数据，必须
     */
    bool inputFrame(const Frame::Ptr &frame) override;

private:
    std::string _config_rtp;
};

} // namespace mediakit

#endif // ZLMEDIAKIT_AV1RTP_H
