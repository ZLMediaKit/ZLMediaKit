#include "JPEG.h"
#include "JPEGRtp.h"
#include "Rtsp/Rtsp.h"
#include "Util/util.h"
#include "Extension/Factory.h"

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
    JPEGSdp(int bitrate) : Sdp(90000, Rtsp::PT_JPEG) {
        _printer << "m=video 0 RTP/AVP " << (int)getPayloadType() << "\r\n";
        if (bitrate) {
            _printer << "b=AS:" << bitrate << "\r\n";
        }
    }

    std::string getSdp() const { return _printer; }

private:
    _StrPrinter _printer;
};

Sdp::Ptr JPEGTrack::getSdp(uint8_t) const {
    return std::make_shared<JPEGSdp>(getBitRate() / 1024);
}


namespace {

CodecId getCodec() {
    return CodecJPEG;
}

Track::Ptr getTrackByCodecId(int sample_rate, int channels, int sample_bit) {
    return std::make_shared<JPEGTrack>();
}

Track::Ptr getTrackBySdp(const SdpTrack::Ptr &track) {
    return std::make_shared<JPEGTrack>();
}

RtpCodec::Ptr getRtpEncoderByCodecId(uint8_t pt) {
    return std::make_shared<JPEGRtpEncoder>();
}

RtpCodec::Ptr getRtpDecoderByCodecId() {
    return std::make_shared<JPEGRtpDecoder>();
}

RtmpCodec::Ptr getRtmpEncoderByTrack(const Track::Ptr &track) {
    WarnL << "Unsupported jpeg rtmp encoder";
    return nullptr;
}

RtmpCodec::Ptr getRtmpDecoderByTrack(const Track::Ptr &track) {
    WarnL << "Unsupported jpeg rtmp decoder";
    return nullptr;
}

Frame::Ptr getFrameFromPtr(const char *data, size_t bytes, uint64_t dts, uint64_t pts) {
    return std::make_shared<JPEGFrame<FrameFromPtr>>(0, CodecJPEG, (char *)data, bytes, dts, pts);
}

} // namespace

CodecPlugin jpeg_plugin = { getCodec,
                            getTrackByCodecId,
                            getTrackBySdp,
                            getRtpEncoderByCodecId,
                            getRtpDecoderByCodecId,
                            getRtmpEncoderByTrack,
                            getRtmpDecoderByTrack,
                            getFrameFromPtr };

} // namespace mediakit
