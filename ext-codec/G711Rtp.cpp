#include "G711Rtp.h"

namespace mediakit {

G711RtpEncoder::G711RtpEncoder(CodecId codec, uint32_t channels){
    _cache_frame = FrameImp::create();
    _cache_frame->_codec_id = codec;
    _channels = channels;
}

void G711RtpEncoder::setOpt(int opt, const toolkit::Any &param) {
    if (opt == RTP_ENCODER_PKT_DUR_MS) {
        if (param.is<uint32_t>()) {
            auto dur = param.get<uint32_t>();
            if (dur < 20 || dur > 180) {
                WarnL << "set g711 rtp encoder  duration ms failed for " << dur;
                return;
            }
            // 向上 20ms 取整
            _pkt_dur_ms = (dur + 19) / 20 * 20;
        }
    }
}

bool G711RtpEncoder::inputFrame(const Frame::Ptr &frame) {
    auto dur = (_cache_frame->size() - _cache_frame->prefixSize()) / (8 * _channels);
    auto next_pts = _cache_frame->pts() + dur;
    if (next_pts == 0) {
        _cache_frame->_pts = frame->pts();
    } else {
        if ((next_pts + _pkt_dur_ms) < frame->pts()) { // 有丢包超过20ms
            _cache_frame->_pts = frame->pts() - dur;
        }
    }
    _cache_frame->_buffer.append(frame->data() + frame->prefixSize(), frame->size() - frame->prefixSize());

    auto stamp = _cache_frame->pts();
    auto ptr = _cache_frame->data() + _cache_frame->prefixSize();
    auto len = _cache_frame->size() - _cache_frame->prefixSize();
    auto remain_size = len;
    size_t max_size = 160 * _channels * _pkt_dur_ms / 20; // 20 ms per 160 byte
    size_t n = 0;
    bool mark = true;
    while (remain_size >= max_size) {
        assert(remain_size >= max_size);
        const size_t rtp_size = max_size;
        n++;
        stamp += _pkt_dur_ms;
        RtpCodec::inputRtp(getRtpInfo().makeRtp(TrackAudio, ptr, rtp_size, mark, stamp), false);
        ptr += rtp_size;
        remain_size -= rtp_size;
    }
    _cache_frame->_buffer.erase(0, n * max_size);
    _cache_frame->_pts += (uint64_t)_pkt_dur_ms * n;
    return len > 0;
}

} // namespace mediakit