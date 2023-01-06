#include "JPEG.h"
#include "Rtsp/Rtsp.h"
#include "Util/util.h"

using namespace toolkit;

namespace mediakit {

bool JPEGTrack::inputFrame(const Frame::Ptr &frame) {
    if (!ready()) {
        if (_height > 0 && _width > 0) {
            if (_tmp == 0) _tmp = frame->dts();
            else _fps = 1000.0 / (frame->dts() - _tmp);
        } else getVideoResolution((uint8_t*)frame->data(), frame->size());
        return false;
    }
    return VideoTrack::inputFrame(frame);
}

void JPEGTrack::getVideoResolution(const uint8_t *buf, int len) {
    for (int i = 0; i < len - 8; i++) {
        if (buf[i] != 0xff)
            continue;
        if (buf[i + 1] == 0xC0 /*SOF0*/) {
            _height = buf[i + 5] * 256 + buf[i + 6];
            _width = buf[i + 7] * 256 + buf[i + 8];
            return;
        }
    }
}

class JPEGSdp : public Sdp {
public:
    JPEGSdp(int bitrate): Sdp(90000, Rtsp::PT_JPEG) {
        _printer << "m=video 0 RTP/AVP " << (int)getPayloadType() << "\r\n";
        if (bitrate) {
            _printer << "b=AS:" << bitrate << "\r\n";
        }
        _printer << "a=control:trackID=" << (int)TrackVideo << "\r\n";
    }

    std::string getSdp() const { return _printer; }

    CodecId getCodecId() const { return CodecJPEG; }

private:
    _StrPrinter _printer;
};

Sdp::Ptr JPEGTrack::getSdp() {
    return std::make_shared<JPEGSdp>(getBitRate() / 1024);
}
} // namespace mediakit
