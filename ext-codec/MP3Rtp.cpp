#include "MP3Rtp.h"
#define MPEG12_HEADER_LEN 4

namespace mediakit {

//MPEG Audio-specific header
/*
 0               1               2               3
 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|             MBZ               |            Frag_offset        |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
*/
    

MP3RtpEncoder::MP3RtpEncoder(int sample_rate, int channels, int sample_bit) {
    _sample_rate = sample_rate;
    _channels = channels;
    _sample_bit = sample_bit;
}

void MP3RtpEncoder::outputRtp(const char *data, size_t len, size_t offset, bool mark, uint64_t stamp) {
    auto rtp = getRtpInfo().makeRtp(TrackAudio, nullptr, len + 4, mark, stamp);
    auto payload = rtp->data() + RtpPacket::kRtpTcpHeaderSize + RtpPacket::kRtpHeaderSize;
    payload[0] = 0;
    payload[1] = 0;
    payload[2] = offset >> 8;
    payload[3] = offset ;
    memcpy(payload + 4, data, len);
    RtpCodec::inputRtp(std::move(rtp), false);
}

bool MP3RtpEncoder::inputFrame(const Frame::Ptr &frame) {
    auto ptr = frame->data() + frame->prefixSize();
    auto size = frame->size() - frame->prefixSize();
    auto remain_size = size;
    auto max_size = getRtpInfo().getMaxSize() - MPEG12_HEADER_LEN;
    int  offset = 0;
    while (remain_size > 0) {
        if (remain_size <= max_size) {
            outputRtp(ptr, remain_size, offset, true, frame->dts());
            break;
        }
        outputRtp(ptr, max_size, offset, false, frame->dts());
        ptr += max_size;
        remain_size -= max_size;
        offset += max_size;
    }
    return true;
}

/////////////////////////////////////////////////////////////////////////////////////

MP3RtpDecoder::MP3RtpDecoder() {
    obtainFrame();
}

void MP3RtpDecoder::obtainFrame() {
    _frame = FrameImp::create();
    _frame->_codec_id = CodecMP3;
}

bool MP3RtpDecoder::inputRtp(const RtpPacket::Ptr &rtp, bool key_pos) {
    auto payload_size = rtp->getPayloadSize();
    if (payload_size <= MPEG12_HEADER_LEN) {
        return false;
    }
    auto stamp = rtp->getStampMS();
    auto ptr = rtp->getPayload();

    _frame->_buffer.append((char *)ptr + MPEG12_HEADER_LEN, payload_size - MPEG12_HEADER_LEN);
    flushData();

    _last_dts = stamp;
    return false;
}

void MP3RtpDecoder::flushData() {
    RtpCodec::inputFrame(_frame);
    obtainFrame();
}

} // namespace mediakit