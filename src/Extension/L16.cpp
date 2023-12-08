/*
 * Copyright (c) 2016 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/xia-chu/ZLMediaKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#include "L16.h"

using namespace std;
using namespace toolkit;

namespace mediakit{

/**
 * L16类型SDP
 */
class L16Sdp : public Sdp {
public:
    /**
     * L16采样位数固定为16位
     * @param payload_type rtp payload type
     * @param channels 通道数
     * @param sample_rate 音频采样率
     * @param bitrate 比特率
     */
    L16Sdp(int payload_type, int sample_rate, int channels, int bitrate) : Sdp(sample_rate, payload_type) {
        _printer << "m=audio 0 RTP/AVP " << payload_type << "\r\n";
        if (bitrate) {
            _printer << "b=AS:" << bitrate << "\r\n";
        }
        _printer << "a=rtpmap:" << payload_type << " " << getCodecName(CodecL16) << "/" << sample_rate  << "/" << channels << "\r\n";
    }

    string getSdp() const override { return _printer; }

private:
    _StrPrinter _printer;
};

Sdp::Ptr L16Track::getSdp(uint8_t payload_type) const {
    WarnL << "Enter  L16Track::getSdp function";
    if (!ready()) {
        WarnL << getCodecName() << " Track未准备好";
        return nullptr;
    }
    return std::make_shared<L16Sdp>(payload_type, getAudioSampleRate(), getAudioChannel(), getBitRate() / 1024);
}

Track::Ptr L16Track::clone() const {
    return std::make_shared<L16Track>(*this);
}

}//namespace mediakit


