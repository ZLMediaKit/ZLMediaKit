/*
 * Copyright (c) 2016-present The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/ZLMediaKit/ZLMediaKit).
 *
 * Use of this source code is governed by MIT-like license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#include "Opus.h"

using namespace std;
using namespace toolkit;

namespace mediakit{

/**
 * Opus类型SDP
 */
class OpusSdp : public Sdp {
public:
    /**
     * 构造opus sdp
     * @param payload_type rtp payload type
     * @param sample_rate 音频采样率
     * @param channels 通道数
     * @param bitrate 比特率
     */
    OpusSdp(int payload_type, int sample_rate, int channels, int bitrate) : Sdp(sample_rate, payload_type) {
        _printer << "m=audio 0 RTP/AVP " << payload_type << "\r\n";
        if (bitrate) {
            _printer << "b=AS:" << bitrate << "\r\n";
        }
        _printer << "a=rtpmap:" << payload_type << " " << getCodecName(CodecOpus) << "/" << sample_rate  << "/" << channels << "\r\n";
    }

    string getSdp() const override {
        return _printer;
    }

private:
    _StrPrinter _printer;
};

Sdp::Ptr OpusTrack::getSdp(uint8_t payload_type) const {
    if (!ready()) {
        WarnL << getCodecName() << " Track未准备好";
        return nullptr;
    }
    return std::make_shared<OpusSdp>(payload_type, getAudioSampleRate(), getAudioChannel(), getBitRate() / 1024);
}

}//namespace mediakit