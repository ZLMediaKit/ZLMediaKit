/*
 * Copyright (c) 2016-present The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/ZLMediaKit/ZLMediaKit).
 *
 * Use of this source code is governed by MIT-like license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#include "MP2A.h"
#include "MP2ARtp.h"
#include "Extension/Factory.h"
#include "Extension/CommonRtmp.h"
#include "Rtsp/Rtsp.h"

using namespace std;
using namespace toolkit;

namespace mediakit {

// ======================== MpegAudioFrameInfo ========================

// MPEG Audio 版本表
// MPEG Audio version table
// Index: version_bits (2 bits from header)
// 00 = MPEG 2.5, 01 = reserved, 10 = MPEG 2, 11 = MPEG 1
static const int s_mpeg_version[] = { 3, 0, 2, 1 }; // 3=MPEG2.5, 0=reserved, 2=MPEG2, 1=MPEG1

// Layer 表: 00=reserved, 01=III, 10=II, 11=I
static const int s_mpeg_layer[] = { 0, 3, 2, 1 };

// MPEG-1 比特率表 (kbps)
// bitrate_index: 0-15, layer: 1-3
static const int s_bitrate_mpeg1[][16] = {
    // Layer I
    { 0, 32, 64, 96, 128, 160, 192, 224, 256, 288, 320, 352, 384, 416, 448, 0 },
    // Layer II
    { 0, 32, 48, 56, 64, 80, 96, 112, 128, 160, 192, 224, 256, 320, 384, 0 },
    // Layer III
    { 0, 32, 40, 48, 56, 64, 80, 96, 112, 128, 160, 192, 224, 256, 320, 0 },
};

// MPEG-2/2.5 比特率表 (kbps)
static const int s_bitrate_mpeg2[][16] = {
    // Layer I
    { 0, 32, 48, 56, 64, 80, 96, 112, 128, 144, 160, 176, 192, 224, 256, 0 },
    // Layer II / III
    { 0, 8, 16, 24, 32, 40, 48, 56, 64, 80, 96, 112, 128, 144, 160, 0 },
};

// 采样率表 (Hz)
// Index: [version_index][samplerate_index]
static const int s_sample_rate[][4] = {
    { 44100, 48000, 32000, 0 }, // MPEG-1
    { 22050, 24000, 16000, 0 }, // MPEG-2
    { 11025, 12000,  8000, 0 }, // MPEG-2.5
};

bool MpegAudioFrameInfo::parse(const uint8_t *data, size_t size, MpegAudioFrameInfo &info) {
    if (size < 4) {
        return false;
    }
    // 检查同步字 0xFFE0 (11 bits all 1)
    if (data[0] != 0xFF || (data[1] & 0xE0) != 0xE0) {
        return false;
    }

    int version_bits = (data[1] >> 3) & 0x03;
    int layer_bits = (data[1] >> 1) & 0x03;
    // int protection = !(data[1] & 0x01);
    int bitrate_index = (data[2] >> 4) & 0x0F;
    int samplerate_index = (data[2] >> 2) & 0x03;
    int padding = (data[2] >> 1) & 0x01;
    int channel_mode = (data[3] >> 6) & 0x03;

    int ver = s_mpeg_version[version_bits];
    int layer = s_mpeg_layer[layer_bits];

    if (ver == 0 || layer == 0 || samplerate_index == 3 || bitrate_index == 0 || bitrate_index == 15) {
        return false;
    }

    int ver_index = ver - 1; // 0=MPEG1, 1=MPEG2, 2=MPEG2.5
    int sr = s_sample_rate[ver_index][samplerate_index];
    if (sr == 0) {
        return false;
    }

    int bitrate = 0;
    if (ver == 1) {
        // MPEG-1
        bitrate = s_bitrate_mpeg1[layer - 1][bitrate_index];
    } else {
        // MPEG-2 / MPEG-2.5
        if (layer == 1) {
            bitrate = s_bitrate_mpeg2[0][bitrate_index];
        } else {
            bitrate = s_bitrate_mpeg2[1][bitrate_index];
        }
    }

    info.version = ver;
    info.layer = layer;
    info.bitrate = bitrate;
    info.sample_rate = sr;
    info.channels = (channel_mode == 3) ? 1 : 2; // 3=mono, 其他=stereo

    // 计算每帧的采样数和帧大小
    if (layer == 1) {
        // Layer I: 384 samples
        info.samples_per_frame = 384;
        info.frame_size = (12 * bitrate * 1000 / sr + padding) * 4;
    } else if (layer == 2) {
        // Layer II: 1152 samples
        info.samples_per_frame = 1152;
        info.frame_size = 144 * bitrate * 1000 / sr + padding;
    } else {
        // Layer III
        if (ver == 1) {
            info.samples_per_frame = 1152;
            info.frame_size = 144 * bitrate * 1000 / sr + padding;
        } else {
            info.samples_per_frame = 576;
            info.frame_size = 72 * bitrate * 1000 / sr + padding;
        }
    }
    return true;
}

// ======================== MP2ATrack ========================

bool MP2ATrack::inputFrame(const Frame::Ptr &frame) {
    if (!_info_parsed) {
        auto data = (const uint8_t *)frame->data() + frame->prefixSize();
        auto size = frame->size() - frame->prefixSize();
        MpegAudioFrameInfo info;
        if (MpegAudioFrameInfo::parse(data, size, info)) {
            _sample_rate = info.sample_rate;
            _channels = info.channels;
            _info_parsed = true;
        }
    }
    return AudioTrackImp::inputFrame(frame);
}

Sdp::Ptr MP2ATrack::getSdp(uint8_t pt) const {
    // RFC 2250/3551: MPA 的 RTP 时钟频率固定为 90000，而不是音频采样率
    // RFC 2250/3551: MPA RTP clock rate is fixed at 90000, not the audio sample rate
    class MP2ASdp : public Sdp {
    public:
        // 注意：Sdp 基类构造必须传入 90000 作为 sample_rate
        MP2ASdp(uint8_t payload_type, int channels, int bitrate)
            : Sdp(90000, payload_type) {
            _printer << "m=audio 0 RTP/AVP " << (int)payload_type << "\r\n";
            if (bitrate) {
                _printer << "b=AS:" << bitrate << "\r\n";
            }
            _printer << "a=rtpmap:" << (int)payload_type << " MPA/90000/" << channels << "\r\n";
        }
        std::string getSdp() const override { return _printer; }

    private:
        toolkit::_StrPrinter _printer;
    };
    return std::make_shared<MP2ASdp>(pt, getAudioChannel(), getBitRate() >> 10);
}

Track::Ptr MP2ATrack::clone() const {
    return std::make_shared<MP2ATrack>(*this);
}

namespace {

CodecId getCodec() {
    return CodecMP2A;
}

Track::Ptr getTrackByCodecId(int sample_rate, int channels, int sample_bit) {
    return std::make_shared<MP2ATrack>(sample_rate, channels);
}

Track::Ptr getTrackBySdp(const SdpTrack::Ptr &track) {
    return std::make_shared<MP2ATrack>(track->_samplerate, track->_channel);
}

RtpCodec::Ptr getRtpEncoderByCodecId(uint8_t pt) {
    return std::make_shared<MP2ARtpEncoder>();
}

RtpCodec::Ptr getRtpDecoderByCodecId() {
    return std::make_shared<MP2ARtpDecoder>();
}

RtmpCodec::Ptr getRtmpEncoderByTrack(const Track::Ptr &track) {
    return std::make_shared<CommonRtmpEncoder>(track);
}

RtmpCodec::Ptr getRtmpDecoderByTrack(const Track::Ptr &track) {
    return std::make_shared<CommonRtmpDecoder>(track);
}

Frame::Ptr getFrameFromPtr(const char *data, size_t bytes, uint64_t dts, uint64_t pts) {
    return std::make_shared<MP2AFrameNoCacheAble>((char *)data, bytes, dts, pts);
}

} // namespace

CodecPlugin mp2a_plugin = { getCodec,
                             getTrackByCodecId,
                             getTrackBySdp,
                             getRtpEncoderByCodecId,
                             getRtpDecoderByCodecId,
                             getRtmpEncoderByTrack,
                             getRtmpDecoderByTrack,
                             getFrameFromPtr };

} // namespace mediakit
