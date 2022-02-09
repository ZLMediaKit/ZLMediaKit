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
     * @param codecId CodecL16
     * @param sample_rate 音频采样率
     * @param payload_type rtp payload
     * @param bitrate 比特率
     */
    L16Sdp(CodecId codecId,
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

Sdp::Ptr L16Track::getSdp() {
    WarnL << "Enter  L16Track::getSdp function";
    if(!ready()){
        WarnL << getCodecName() << " Track未准备好";
        return nullptr;
    }
    return std::make_shared<L16Sdp>(getCodecId(), getAudioSampleRate(), getAudioChannel(), getBitRate() / 1024);
}

Track::Ptr L16Track::clone() {
    return std::make_shared<std::remove_reference<decltype(*this)>::type >(*this);
}

}//namespace mediakit


