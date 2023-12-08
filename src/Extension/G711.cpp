/*
 * Copyright (c) 2016 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/xia-chu/ZLMediaKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#include "G711.h"

using namespace std;
using namespace toolkit;

namespace mediakit{

/**
 * G711类型SDP
 */
class G711Sdp : public Sdp {
public:
    /**
     * G711采样率固定为8000
     * @param codecId G711A G711U
     * @param payload_type rtp payload type
     * @param sample_rate 音频采样率
     * @param channels 通道数
     * @param bitrate 比特率
     */
    G711Sdp(CodecId codecId, int payload_type, int sample_rate, int channels, int bitrate)
        : Sdp(sample_rate, payload_type) {
        _printer << "m=audio 0 RTP/AVP " << payload_type << "\r\n";
        if (bitrate) {
            _printer << "b=AS:" << bitrate << "\r\n";
        }
        _printer << "a=rtpmap:" << payload_type << " " << getCodecName(codecId) << "/" << sample_rate  << "/" << channels << "\r\n";
    }

    string getSdp() const override {
        return _printer;
    }

private:
    _StrPrinter _printer;
};

Track::Ptr G711Track::clone() const {
    return std::make_shared<G711Track>(*this);
}

Sdp::Ptr G711Track::getSdp(uint8_t payload_type) const {
    if (!ready()) {
        WarnL << getCodecName() << " Track未准备好";
        return nullptr;
    }

    const auto codec = getCodecId();
    const auto sample_rate = getAudioSampleRate();
    const auto audio_channel = getAudioChannel();
    const auto bitrate = getBitRate() >> 10;
    if (sample_rate == 8000 && audio_channel == 1) {
        // https://datatracker.ietf.org/doc/html/rfc3551#section-6
        payload_type = (codec == CodecG711U) ? Rtsp::PT_PCMU : Rtsp::PT_PCMA;
    }

    return std::make_shared<G711Sdp>(codec, payload_type, sample_rate, audio_channel, bitrate);
}

}//namespace mediakit


