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
     * @param sample_rate 音频采样率
     * @param payload_type rtp payload
     * @param bitrate 比特率
     */
    G711Sdp(CodecId codecId,
            int sample_rate,
            int channels,
            int bitrate = 128,
            int payload_type = 98) : Sdp(sample_rate,payload_type), _codecId(codecId){
        _printer << "m=audio 0 RTP/AVP " << payload_type << "\r\n";
        if (bitrate) {
            _printer << "b=AS:" << bitrate << "\r\n";
        }
        _printer << "a=rtpmap:" << payload_type << " " << getCodecName() << "/" << sample_rate  << "/" << channels << "\r\n";
        _printer << "a=control:trackID=" << (int)TrackAudio << "\r\n";
    }

    string getSdp() const override {
        return _printer;
    }

    CodecId getCodecId() const override {
        return _codecId;
    }
private:
    _StrPrinter _printer;
    CodecId _codecId;
};

Track::Ptr G711Track::clone() {
    return std::make_shared<std::remove_reference<decltype(*this)>::type>(*this);
}

Sdp::Ptr G711Track::getSdp() {
    if(!ready()){
        WarnL << getCodecName() << " Track未准备好";
        return nullptr;
    }

    const auto codec = getCodecId();
    const auto sample_rate = getAudioSampleRate();
    const auto audio_channel = getAudioChannel();
    const auto bitrate = getBitRate() >> 10;
    auto payload_type = 98;
    if (sample_rate == 8000 && audio_channel == 1) {
        // https://datatracker.ietf.org/doc/html/rfc3551#section-6
        payload_type = (codec == CodecG711U) ? Rtsp::PT_PCMU : Rtsp::PT_PCMA;
    }

    return std::make_shared<G711Sdp>(codec, sample_rate, audio_channel, bitrate, payload_type);
}

}//namespace mediakit


