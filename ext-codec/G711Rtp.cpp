#include "G711Rtp.h"

namespace mediakit {

G711RtpEncoder::G711RtpEncoder(int sample_rate, int channels, int sample_bit) {
    _sample_rate = sample_rate;
    _channels = channels;
    _sample_bit = sample_bit;
}

void G711RtpEncoder::setOpt(int opt, const toolkit::Any &param) {
    if (opt == RTP_ENCODER_PKT_DUR_MS) {
        if (param.is<uint32_t>()) {
            auto dur = param.get<uint32_t>();
            if (dur < 20 || dur > 180) {
                WarnL << "set g711 rtp encoder  duration ms failed for " << dur;
                return;
            }
            // 向上 20ms 取整  [AUTO-TRANSLATED:b8a9e39e]
            // Round up to the nearest 20ms
            _pkt_dur_ms = (dur + 19) / 20 * 20;
        }
    }
}

bool G711RtpEncoder::inputFrame(const Frame::Ptr &frame) {
    auto ptr = frame->data() + frame->prefixSize();
    auto size = frame->size() - frame->prefixSize();
    uint64_t in_pts;
    if (!_pkt_bytes) {
        in_pts = frame->pts();
        // G711压缩率固定是2倍
        _pkt_bytes = _pkt_dur_ms * _channels * (_sample_bit / 8) * _sample_rate / 1000 / 2;
    } else {
        in_pts = frame->pts() - _buffer.size() * _pkt_dur_ms / _pkt_bytes;
    }

    _buffer.append(ptr, size);

    while (_buffer.size() >= _pkt_bytes) {
        auto tmp = (in_pts+_pkt_dur_ms-1)/_pkt_dur_ms*_pkt_dur_ms;
        RtpCodec::inputRtp(getRtpInfo().makeRtp(TrackAudio, _buffer.data(), _pkt_bytes, false, tmp), false);
         in_pts += _pkt_dur_ms;
        _buffer.erase(0, _pkt_bytes);
    }
    return true;
}

} // namespace mediakit