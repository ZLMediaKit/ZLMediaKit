#include "G711ToAAC.h"

#include "AACRtp.h"
#include "AACRtmp.h"
#include "Common/Parser.h"
#include "Extension/Factory.h"

#include "EasyAACEncoder/EasyAACEncoderAPI.h"

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

CodecId G711ToAACTrack::getCodec() {

     InfoL << "CodecG711ToAAC codecid: CodecG711ToAAC " << getCodecId() 
        << " getTrackType:" << getTrackType() 
        << " getCodecName:" << getCodecName() 
        << " getTrackTypeStr:" << getTrackTypeStr()
        << " getBitRate:" << getBitRate();

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

    #define tempbuffer_size 4096

    auto frame_aac = FrameImp::create();
    frame_aac->_codec_id = CodecAAC;
    frame_aac->_prefix_size = ADTS_HEADER_LEN;
    frame_aac->_dts = frame->dts();
    frame_aac->setIndex(frame->getIndex());

    int ret = 0;
    unsigned int outlen = 0;
    if(frame->size()<tempbuffer_size) {
        char tempbuffer[frame->size()];
        int ret = Easy_AACEncoder_Encode(_AACEncoderHandle, (unsigned char*)frame->data(), frame->size(), (unsigned char*)tempbuffer, &outlen);
        frame_aac->_buffer.assign(tempbuffer, outlen);
    } else {
        char* G711ABuffer_ = new char[frame->size()];
        int ret = Easy_AACEncoder_Encode(_AACEncoderHandle, (unsigned char*)frame->data(), frame->size(), (unsigned char*)G711ABuffer_, &outlen);
        frame_aac->_buffer.assign(G711ABuffer_, outlen);
        delete[] G711ABuffer_;
    }

    InfoL<< "G711ToAACTrack::inputFrame frame size:" << frame->size() 
        << " prefixSize:" << frame->prefixSize() 
        << " dts:" << frame->dts() 
        << " pts:" << frame->pts()
        << " outlen:" << outlen
        << " ret:" << ret;

    auto ptr = reinterpret_cast<const uint8_t *>(frame_aac->data());

    if ((ptr[0] == 0xFF && (ptr[1] & 0xF0) == 0xF0) && frame->size() > ADTS_HEADER_LEN) {
        // adts头打入了rtp包，不符合规范，兼容EasyPusher的bug  [AUTO-TRANSLATED:203a5ee9]
        // The adts header is inserted into the rtp packet, which is not compliant with the specification, compatible with the bug of EasyPusher
        frame_aac->_prefix_size = ADTS_HEADER_LEN;
        // InfoL << "G711ToAACTrack ADTS_HEADER_LEN" << ADTS_HEADER_LEN << ", frame->size() " << frame->size() 
        //     << " prefixSize:" << frame->prefixSize() 
        //     << " dts:" << frame->dts() 
        //     << " pts:" << frame->pts();
    } else {
        // InfoL << "G711ToAACTrack::inputFrame frame size:" << frame->size() 
        //     << " prefixSize:" << frame->prefixSize() 
        //     << " dts:" << frame->dts() 
        //     << " pts:" << frame->pts();
    }

    return AACTrack::inputFrame(frame_aac);
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
