/*
 * Copyright (c) 2016-present The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/ZLMediaKit/ZLMediaKit).
 *
 * Use of this source code is governed by MIT-like license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#include "G722.h"
#include "Rtsp/Rtsp.h"
#include "Util/util.h"

using namespace std;
using namespace toolkit;

namespace mediakit {

// G722 SDP实现
Sdp::Ptr G722Track::getSdp(uint8_t payload_type) const {
    // G.722 使用动态payload type 9 (RFC 3551)
    // 或者动态payload type
    class G722Sdp : public Sdp {
    public:
        G722Sdp(int sample_rate, int channels, uint8_t payload_type)
            : Sdp(sample_rate, payload_type), _channels(channels) {}
        
        string getSdp() const override {
            _StrPrinter printer;
            // G.722的采样率在SDP中标注为8000（历史原因），但实际是16kHz
            printer << "m=audio 0 RTP/AVP " << (int)getPayloadType() << "\r\n";
            printer << "a=rtpmap:" << (int)getPayloadType() << " G722/8000";
            if (_channels > 1) {
                printer << "/" << _channels;
            }
            printer << "\r\n";
            return std::move(printer);
        }
    private:
        int _channels;
    };
    
    return std::make_shared<G722Sdp>(getAudioSampleRate(), getAudioChannel(), 
                                      payload_type ? payload_type : 9);
}

} // namespace mediakit
