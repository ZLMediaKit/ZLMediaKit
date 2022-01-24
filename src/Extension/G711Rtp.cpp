#include "G711Rtp.h"

namespace mediakit {

G711RtpEncoder::G711RtpEncoder(
    CodecId codec, uint32_t ssrc, uint32_t mtu_size, uint32_t sample_rate, uint8_t payload_type, uint8_t interleaved,
    uint32_t channels)
    : CommonRtpDecoder(codec)
    , RtpInfo(ssrc, mtu_size, sample_rate, payload_type, interleaved) {
    _cache_frame = FrameImp::create();
    _cache_frame->_codec_id = codec;
    _channels = channels;
}

bool G711RtpEncoder::inputFrame(const Frame::Ptr &frame) {
    auto dur = (_cache_frame->size() - _cache_frame->prefixSize()) / (8 * _channels);
    auto next_pts = _cache_frame->pts() + dur;
    if (next_pts == 0) {
        _cache_frame->_pts = frame->pts();
    } else {
        if ((next_pts + 20) < frame->pts()) { // 有丢包超过20ms
            _cache_frame->_pts = frame->pts() - dur;
        }
    }
    _cache_frame->_buffer.append(frame->data() + frame->prefixSize(), frame->size() - frame->prefixSize());

    auto stamp = _cache_frame->pts();
    auto ptr = _cache_frame->data() + _cache_frame->prefixSize();
    auto len = _cache_frame->size() - _cache_frame->prefixSize();
    auto remain_size = len;
    auto max_size = 160 * _channels; // 20 ms per rtp
    int n = 0;
    bool mark = false;
    while (remain_size >= max_size) {
        size_t rtp_size;
        if (remain_size >= max_size) {
            rtp_size = max_size;
        } else {
            break;
        }
        n++;
        stamp += 20;
        RtpCodec::inputRtp(makeRtp(getTrackType(), ptr, rtp_size, mark, stamp), false);
        ptr += rtp_size;
        remain_size -= rtp_size;
    }
    _cache_frame->_buffer.erase(0, n * max_size);
    _cache_frame->_pts += 20 * n;
    return len > 0;
}

} // namespace mediakit