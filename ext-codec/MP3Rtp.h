/*
 * Copyright (c) 2016-present The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/ZLMediaKit/ZLMediaKit).
 *
 * Use of this source code is governed by MIT-like license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#ifndef ZLMEDIAKIT_MP3RTP_H
#define ZLMEDIAKIT_MP3RTP_H

#include "Rtsp/RtpCodec.h"
#include "Extension/Frame.h"
#include "Extension/CommonRtp.h"

namespace mediakit {

class MP3RtpEncoder : public RtpCodec {
public:
    using Ptr = std::shared_ptr<MP3RtpEncoder>;

    MP3RtpEncoder(int sample_rate = 44100, int channels = 2, int sample_bit = 16);

    void outputRtp(const char *data, size_t len, size_t offset, bool mark, uint64_t stamp);

    bool inputFrame(const Frame::Ptr &frame) override;

private:
    int _channels;
    int _sample_rate;
    int _sample_bit;
};


class MP3RtpDecoder : public RtpCodec {
public:
    using Ptr = std::shared_ptr<MP3RtpDecoder>;

    MP3RtpDecoder();

    bool inputRtp(const RtpPacket::Ptr &rtp, bool key_pos = false) override;

private:
    void obtainFrame();

    void flushData();


private:
    uint64_t _last_dts = 0;
    FrameImp::Ptr _frame;
};

}//namespace mediakit
#endif //ZLMEDIAKIT_MP3RTP_H
