#include "G711ToAAC.h"

#include "AACRtp.h"
#include "AACRtmp.h"
#include "Common/Parser.h"
#include "Extension/Factory.h"

#include "EasyAACEncoder/EasyAACEncoderAPI.h"

// Inline ADTS frame-length parser (mirrors the one in AAC.cpp)
static int getAdtsFrameLength(const uint8_t *data, size_t bytes) {
    if (bytes < 7) return -1;
    if (0xFF != data[0] || 0xF0 != (data[1] & 0xF0)) return -1;
    uint16_t len = ((uint16_t)(data[3] & 0x03) << 11) | ((uint16_t)data[4] << 3) | ((uint16_t)(data[5] >> 5) & 0x07);
    return len;
}

#ifdef ENABLE_MP4
#include "mpeg4-aac.h"
#endif

using namespace std;
using namespace toolkit;

namespace mediakit {

static string G711ToAACTrackCfg (int sample_rate, int channels, int sample_bit) {
    struct mpeg4_aac_t aac;
    memset(&aac, 0, sizeof(aac));

    aac.profile = 2; // AAC LC
    aac.sampling_frequency_index = 11;     // 11 = 48kHz
    aac.channel_configuration = channels;  // 1
    aac.sampling_frequency = sample_rate;
    aac.channels = channels;

    assert(aac.profile > 0 && aac.profile < 31);
	assert(aac.channel_configuration >= 0 && aac.channel_configuration <= 7);
	assert(aac.sampling_frequency_index >= 0 && aac.sampling_frequency_index <= 0xc);
	aac.channels = mpeg4_aac_channel_count(aac.channel_configuration);
	aac.sampling_frequency = mpeg4_aac_audio_frequency_to((mpeg4_aac_frequency)aac.sampling_frequency_index);
	aac.extension_frequency = aac.sampling_frequency;

    char buf[32] = {0};
    int len = mpeg4_aac_audio_specific_config_save(&aac, (uint8_t *) buf, sizeof(buf));
    if (len > 0) {
        return string(buf, len);
    }

    WarnL << "生成aac config失败, adts header:" << hexdump(buf, sizeof(buf));
    return "";
}
G711ToAACTrack::G711ToAACTrack(int sample_rate, int channels, int sample_bit) : AACTrack(G711ToAACTrackCfg(sample_rate, channels, sample_bit)) {
    if (sample_rate != 8000) {
        // throw std::invalid_argument("G711ToAACTrack only support 8k/16k/32k");
        WarnL << "G711ToAACTrack only support 8k/16k/32k";
    }
    if (channels != 1) {
        // throw std::invalid_argument("G711ToAACTrack only support mono");
        WarnL << "G711ToAACTrack only support mono";
    }

    _ucAudioCodec = Law_ALaw; // Law_uLaw
    _ucAudioChannel = channels; // 1
    _u32AudioSamplerate = sample_rate; // 8000
    _u32PCMBitSize = sample_bit; // 16
}

G711ToAACTrack::G711ToAACTrack(const string &aac_cfg)
     : AACTrack(aac_cfg) {
}

G711ToAACTrack::~G711ToAACTrack() {

    WarnL << "this << " << this << " AACEncoderHandle_:" << _AACEncoderHandle;

    if (_AACEncoderHandle) {
        Easy_AACEncoder_Release(_AACEncoderHandle);
        _AACEncoderHandle = nullptr;
    }
}

Track::Ptr G711ToAACTrack::clone() const {
    return std::make_shared<G711ToAACTrack>(*this);
}

CodecId G711ToAACTrack::getCodecId() const {

    // InfoL << "codecid: CodecG711ToAAC getTrackType:" << getTrackType();
    //     << " getCodecName:" << getCodecName() 
    //     << " getTrackTypeStr:" << getTrackTypeStr()
    //     << " getBitRate:" << getBitRate();

     return CodecG711ToAAC;
}

bool G711ToAACTrack::inputFrame(const Frame::Ptr &frame) {

    if (!_AACEncoderHandle) {
        
        InitParam initParam;
        initParam.ucAudioCodec = _ucAudioCodec;
        initParam.ucAudioChannel = _ucAudioChannel;
        initParam.u32AudioSamplerate = _u32AudioSamplerate;
        initParam.u32PCMBitSize = _u32PCMBitSize;

        _AACEncoderHandle = Easy_AACEncoder_Init(initParam);
    }

    // #define tempbuffer_size 4096

    
    assert(frame->size());

    if(frame->size()>tempbuffer_size) {
        // 如果G711帧过大，直接丢弃  [AUTO-TRANSLATED:3c0b1f2d]
        // If the G711 frame is too large, discard it directly
        WarnL << "G711帧过大" << frame->size() << ", dts:" << frame->dts() << ", pts:" << frame->pts();
        // return false;
    }

    unsigned int outlen = tempbuffer_size;

    int ret = Easy_AACEncoder_Encode(_AACEncoderHandle, (unsigned char*)frame->data(), frame->size(), (unsigned char*)_ToAAcBuffer, &outlen);
    if(ret<=0||outlen==0) {
        // WarnL << "Easy_AACEncoder_Encode ret:" << ret << ", outlen:" << outlen;
        return false;
    }

    // Easy_AACEncoder_Encode may produce multiple concatenated ADTS frames in one call
    // (internal ring buffer drains via while-loop). Manually split so each sub-frame is
    // fed individually to AACTrack::inputFrame, and attach the original G711 frame as
    // origin only to the FIRST sub-frame — subsequent sub-frames have no origin and will
    // be skipped by the null guard in MultiMediaSourceMuxer::onTrackFrame_l.
    bool result = false;
    bool is_first = true;
    int64_t dts = frame->dts();
    int64_t pts = frame->pts();
    auto ptr = reinterpret_cast<const uint8_t *>(_ToAAcBuffer);
    auto end = ptr + outlen;
    while (ptr < end) {
        auto frame_len = getAdtsFrameLength(ptr, end - ptr);
        if (frame_len < (int)ADTS_HEADER_LEN) {
            WarnL << "G711ToAAC: invalid ADTS sync, stop splitting, remaining:" << (end - ptr);
            break;
        }
        if (ptr + frame_len > end) {
            WarnL << "G711ToAAC: ADTS frame_len exceeds buffer, frame_len:" << frame_len;
            break;
        }
        auto sub_frame = FrameImp::create();
        sub_frame->_codec_id = CodecG711ToAAC;
        sub_frame->_prefix_size = ADTS_HEADER_LEN;
        sub_frame->_dts = dts;
        sub_frame->_pts = pts;
        sub_frame->setIndex(frame->getIndex());
        sub_frame->_buffer.assign((char *)ptr, frame_len);
        if (is_first) {
            sub_frame->addOriginFrame(frame);
            is_first = false;
        }
        // Each sub_frame is a single complete ADTS frame, so AACTrack::inputFrame
        // will take the fast path (frame_len == frame->size()) and not re-split.
        if (AACTrack::inputFrame(sub_frame)) {
            result = true;
        }
        ptr += frame_len;
        dts += 1024 * 1000 / getAudioSampleRate();
        pts += 1024 * 1000 / getAudioSampleRate();
    }
    return result;
}

void G711ToAACTrack::addOriginTrack(Track::Ptr track) {
    _originTrack = track;
}

Track::Ptr G711ToAACTrack::getOriginTrack() {
    return _originTrack;
}

void G711ToAACTrack::setIndex(int index) { 
    if (_originTrack) {
        _originTrack->setIndex(index);
    }

    CodecInfo::setIndex(index);
}

namespace {


CodecId getCodec() {
    return CodecG711ToAAC;
}

    
Track::Ptr getTrackByCodecId(int sample_rate, int channels, int sample_bit) {


    return std::make_shared<G711ToAACTrack>(sample_rate, channels, sample_bit);
}

Track::Ptr getTrackBySdp(const SdpTrack::Ptr &track) {
    string aac_cfg_str = findSubString(track->_fmtp.data(), "config=", ";");
    if (aac_cfg_str.empty()) {
        aac_cfg_str = findSubString(track->_fmtp.data(), "config=", nullptr);
    }
    if (aac_cfg_str.empty()) {
        // 如果sdp中获取不到aac config信息，那么在rtp也无法获取，那么忽略该Track  [AUTO-TRANSLATED:995bc20d]
        // If aac config information cannot be obtained from sdp, then it cannot be obtained from rtp either, so ignore this Track
        return nullptr;
    }
    string aac_cfg;
    for (size_t i = 0; i < aac_cfg_str.size() / 2; ++i) {
        unsigned int cfg;
        sscanf(aac_cfg_str.substr(i * 2, 2).data(), "%02X", &cfg);
        cfg &= 0x00FF;
        aac_cfg.push_back((char)cfg);
    }
    return std::make_shared<G711ToAACTrack>(aac_cfg);
}

RtpCodec::Ptr getRtpEncoderByCodecId(uint8_t pt) {
    return std::make_shared<AACRtpEncoder>();
}

RtpCodec::Ptr getRtpDecoderByCodecId() {
    return std::make_shared<AACRtpDecoder>();
}

RtmpCodec::Ptr getRtmpEncoderByTrack(const Track::Ptr &track) {
    return std::make_shared<AACRtmpEncoder>(track);
}

RtmpCodec::Ptr getRtmpDecoderByTrack(const Track::Ptr &track) {
    return std::make_shared<AACRtmpDecoder>(track);
}

size_t aacPrefixSize(const char *data, size_t bytes) {
    uint8_t *ptr = (uint8_t *)data;
    size_t prefix = 0;
    if (!(bytes > ADTS_HEADER_LEN && ptr[0] == 0xFF && (ptr[1] & 0xF0) == 0xF0)) {
        return 0;
    }
    return ADTS_HEADER_LEN;
}

Frame::Ptr getFrameFromPtr(const char *data, size_t bytes, uint64_t dts, uint64_t pts) {
    return std::make_shared<FrameFromPtr>(CodecG711ToAAC, (char *)data, bytes, dts, pts, aacPrefixSize(data, bytes));
}

} // namespace

CodecPlugin g711_to_aac_plugin = { getCodec,
                           getTrackByCodecId,
                           getTrackBySdp,
                           getRtpEncoderByCodecId,
                           getRtpDecoderByCodecId,
                           getRtmpEncoderByTrack,
                           getRtmpDecoderByTrack,
                           getFrameFromPtr };

}
