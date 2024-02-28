#include "G711Rtp.h"

namespace mediakit {

G711RtpEncoder::G711RtpEncoder(CodecId codec, uint32_t channels){
    _cache_frame = FrameImp::create();
    _cache_frame->_codec_id = codec;
    _channels = channels;
}

void G711RtpEncoder::setOpt(int opt,void* option_value,size_t option_len){
    if(opt == RTP_ENCODER_PKT_DUR_MS && option_len == 4 && option_value != NULL){
        _pkt_dur_ms = *((int*)option_value);
        _pkt_dur_ms = (_pkt_dur_ms+19)/20*20;
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
    auto max_size = 160 * _channels * _pkt_dur_ms/20; // 20 ms per rtp
    uint32_t n = 0;
    bool mark = true;
    while (remain_size >= max_size) {
        size_t rtp_size;
        if (remain_size >= max_size) {
            rtp_size = max_size;
        } else {
            break;
        }
        n++;
        stamp += _pkt_dur_ms;
        RtpCodec::inputRtp(getRtpInfo().makeRtp(TrackAudio, ptr, rtp_size, mark, stamp), true);
        ptr += rtp_size;
        remain_size -= rtp_size;
    }
    _cache_frame->_buffer.erase(0, n * max_size);
    _cache_frame->_pts += _pkt_dur_ms * n;
    return len > 0;
}

} // namespace mediakit