/*
 * Copyright (c) 2016-present The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/ZLMediaKit/ZLMediaKit).
 *
 * Use of this source code is governed by MIT-like license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#ifndef ZLMEDIAKIT_PSENCODER_H
#define ZLMEDIAKIT_PSENCODER_H

#if defined(ENABLE_RTPPROXY)

#include "Record/MPEG.h"
#include "Common/MediaSink.h"

namespace mediakit {

class CommonRtpEncoder;

class PSEncoderImp : public MpegMuxer {
public:
    /**
     * 创建psh或ts rtp编码器
     * @param ssrc rtp的ssrc
     * @param payload_type rtp的pt
     * @param ps_or_ts true: ps, false: ts
     * Create a psh or ts rtp encoder
     * @param ssrc rtp's ssrc
     * @param payload_type rtp's pt
     * @param ps_or_ts true: ps, false: ts
     
     * [AUTO-TRANSLATED:b79d8b65]
     */
    PSEncoderImp(uint32_t ssrc, uint8_t payload_type = 96, bool ps_or_ts = true);
    ~PSEncoderImp() override;

protected:
    // rtp打包后回调  [AUTO-TRANSLATED:8f88aef9]
    // Callback after rtp packaging
    virtual void onRTP(toolkit::Buffer::Ptr rtp, bool is_key = false) = 0;

protected:
    void onWrite(std::shared_ptr<toolkit::Buffer> buffer, uint64_t stamp, bool key_pos) override;

private:
    std::shared_ptr<CommonRtpEncoder> _rtp_encoder;
};

}//namespace mediakit

#endif //ENABLE_RTPPROXY
#endif //ZLMEDIAKIT_PSENCODER_H
