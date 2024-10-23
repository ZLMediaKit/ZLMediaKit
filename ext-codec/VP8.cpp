#include "VP8.h"
#include "VP8Rtp.h"
#include "VpxRtmp.h"
#include "Extension/Factory.h"

using namespace std;
using namespace toolkit;

namespace mediakit {

bool VP8Track::inputFrame(const Frame::Ptr &frame) {
    char *dataPtr = frame->data() + frame->prefixSize();
    if (frame->keyFrame()) {
        if (frame->size() - frame->prefixSize() < 10)
            return false;
        _width = ((dataPtr[7] << 8) + dataPtr[6]) & 0x3FFF;
        _height = ((dataPtr[9] << 8) + dataPtr[8]) & 0x3FFF;
        webm_vpx_codec_configuration_record_from_vp8(&_vpx, &_width, &_height, dataPtr, frame->size() - frame->prefixSize());
        // InfoL << _width << "x" << _height;
    }
    return VideoTrackImp::inputFrame(frame);
}

Buffer::Ptr VP8Track::getExtraData() const {
    auto ret = BufferRaw::create(8 + _vpx.codec_intialization_data_size);
    ret->setSize(webm_vpx_codec_configuration_record_save(&_vpx, (uint8_t *)ret->data(), ret->getCapacity()));
    return ret;
}

void VP8Track::setExtraData(const uint8_t *data, size_t size) {
    webm_vpx_codec_configuration_record_load(data, size, &_vpx);
}

namespace {

CodecId getCodec() {
    return CodecVP8;
}

Track::Ptr getTrackByCodecId(int sample_rate, int channels, int sample_bit) {
    return std::make_shared<VP8Track>();
}

Track::Ptr getTrackBySdp(const SdpTrack::Ptr &track) {
    return std::make_shared<VP8Track>();
}

RtpCodec::Ptr getRtpEncoderByCodecId(uint8_t pt) {
    return std::make_shared<VP8RtpEncoder>();
}

RtpCodec::Ptr getRtpDecoderByCodecId() {
    return std::make_shared<VP8RtpDecoder>();
}

RtmpCodec::Ptr getRtmpEncoderByTrack(const Track::Ptr &track) {
    return std::make_shared<VpxRtmpEncoder>(track);
}

RtmpCodec::Ptr getRtmpDecoderByTrack(const Track::Ptr &track) {
    return std::make_shared<VpxRtmpDecoder>(track);
}

Frame::Ptr getFrameFromPtr(const char *data, size_t bytes, uint64_t dts, uint64_t pts) {
    return std::make_shared<VP8FrameNoCacheAble>((char *)data, bytes, dts, pts, 0);
}

} // namespace

CodecPlugin vp8_plugin = { getCodec,
                            getTrackByCodecId,
                            getTrackBySdp,
                            getRtpEncoderByCodecId,
                            getRtpDecoderByCodecId,
                            getRtmpEncoderByTrack,
                            getRtmpDecoderByTrack,
                            getFrameFromPtr };

} // namespace mediakit