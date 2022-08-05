/*
 * Copyright (c) 2016 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/xia-chu/ZLMediaKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#ifndef ZLMEDIAKIT_RAWENCODER_H
#define ZLMEDIAKIT_RAWENCODER_H

#if defined(ENABLE_RTPPROXY)

#include "Common/MediaSink.h"
#include "Common/Stamp.h"
#include "Extension/CommonRtp.h"

namespace mediakit {

class RawEncoderImp : public MediaSinkInterface {
public:
    RawEncoderImp(uint32_t ssrc, uint8_t payload_type = 96, bool send_audio = true);
    ~RawEncoderImp() override;

    /**
     * 添加音视频轨道
     */
    bool addTrack(const Track::Ptr &track) override;

    /**
     * 重置音视频轨道
     */
    void resetTracks() override;

    /**
     * 输入帧数据
     */
    bool inputFrame(const Frame::Ptr &frame) override;

protected:
    // rtp打包后回调
    virtual void onRTP(toolkit::Buffer::Ptr rtp, bool is_key = false) = 0;

private:
    RtpCodec::Ptr createRtpEncoder(const Track::Ptr &track);

private:
    bool _send_audio;
    uint8_t _payload_type;
    uint32_t _ssrc;
    RtpCodec::Ptr _rtp_encoder;
};

} // namespace mediakit

#endif // ENABLE_RTPPROXY
#endif // ZLMEDIAKIT_RAWENCODER_H
