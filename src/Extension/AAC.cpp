/*
 * Copyright (c) 2016 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/xia-chu/ZLMediaKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#include "AAC.h"
#ifdef ENABLE_MP4
#include "mpeg4-aac.h"
#endif

using namespace std;
using namespace toolkit;

namespace mediakit{

#ifndef ENABLE_MP4
unsigned const samplingFrequencyTable[16] = { 96000, 88200, 64000, 48000, 44100, 32000, 24000, 22050, 16000, 12000, 11025, 8000, 7350, 0, 0, 0 };

class AdtsHeader{
public:
    unsigned int syncword = 0; //12 bslbf 同步字The bit string ‘1111 1111 1111’，说明一个ADTS帧的开始
    unsigned int id;        //1 bslbf   MPEG 标示符, 设置为1
    unsigned int layer;    //2 uimsbf Indicates which layer is used. Set to ‘00’
    unsigned int protection_absent;  //1 bslbf  表示是否误码校验
    unsigned int profile; //2 uimsbf  表示使用哪个级别的AAC，如01 Low Complexity(LC)--- AACLC
    unsigned int sf_index;           //4 uimsbf  表示使用的采样率下标
    unsigned int private_bit;        //1 bslbf
    unsigned int channel_configuration;  //3 uimsbf  表示声道数
    unsigned int original;               //1 bslbf
    unsigned int home;                   //1 bslbf
    //下面的为改变的参数即每一帧都不同
    unsigned int copyright_identification_bit;   //1 bslbf
    unsigned int copyright_identification_start; //1 bslbf
    unsigned int aac_frame_length; // 13 bslbf  一个ADTS帧的长度包括ADTS头和raw data block
    unsigned int adts_buffer_fullness;           //11 bslbf     0x7FF 说明是码率可变的码流
    //no_raw_data_blocks_in_frame 表示ADTS帧中有number_of_raw_data_blocks_in_frame + 1个AAC原始帧.
    //所以说number_of_raw_data_blocks_in_frame == 0
    //表示说ADTS帧中有一个AAC数据块并不是说没有。(一个AAC原始帧包含一段时间内1024个采样及相关数据)
    unsigned int no_raw_data_blocks_in_frame;    //2 uimsfb
};

static void dumpAdtsHeader(const AdtsHeader &hed, uint8_t *out) {
    out[0] = (hed.syncword >> 4 & 0xFF); //8bit
    out[1] = (hed.syncword << 4 & 0xF0); //4 bit
    out[1] |= (hed.id << 3 & 0x08); //1 bit
    out[1] |= (hed.layer << 1 & 0x06); //2bit
    out[1] |= (hed.protection_absent & 0x01); //1 bit
    out[2] = (hed.profile << 6 & 0xC0); // 2 bit
    out[2] |= (hed.sf_index << 2 & 0x3C); //4bit
    out[2] |= (hed.private_bit << 1 & 0x02); //1 bit
    out[2] |= (hed.channel_configuration >> 2 & 0x03); //1 bit
    out[3] = (hed.channel_configuration << 6 & 0xC0);  // 2 bit
    out[3] |= (hed.original << 5 & 0x20);				//1 bit
    out[3] |= (hed.home << 4 & 0x10);				//1 bit
    out[3] |= (hed.copyright_identification_bit << 3 & 0x08);			//1 bit
    out[3] |= (hed.copyright_identification_start << 2 & 0x04);		//1 bit
    out[3] |= (hed.aac_frame_length >> 11 & 0x03);				//2 bit
    out[4] = (hed.aac_frame_length >> 3 & 0xFF);				//8 bit
    out[5] = (hed.aac_frame_length << 5 & 0xE0);				//3 bit
    out[5] |= (hed.adts_buffer_fullness >> 6 & 0x1F);				//5 bit
    out[6] = (hed.adts_buffer_fullness << 2 & 0xFC);				//6 bit
    out[6] |= (hed.no_raw_data_blocks_in_frame & 0x03);				//2 bit
}

static void parseAacConfig(const string &config, AdtsHeader &adts) {
    uint8_t cfg1 = config[0];
    uint8_t cfg2 = config[1];

    int audioObjectType;
    int sampling_frequency_index;
    int channel_configuration;

    audioObjectType = cfg1 >> 3;
    sampling_frequency_index = ((cfg1 & 0x07) << 1) | (cfg2 >> 7);
    channel_configuration = (cfg2 & 0x7F) >> 3;

    adts.syncword = 0x0FFF;
    adts.id = 0;
    adts.layer = 0;
    adts.protection_absent = 1;
    adts.profile = audioObjectType - 1;
    adts.sf_index = sampling_frequency_index;
    adts.private_bit = 0;
    adts.channel_configuration = channel_configuration;
    adts.original = 0;
    adts.home = 0;
    adts.copyright_identification_bit = 0;
    adts.copyright_identification_start = 0;
    adts.aac_frame_length = 7;
    adts.adts_buffer_fullness = 2047;
    adts.no_raw_data_blocks_in_frame = 0;
}
#endif// ENABLE_MP4

int getAacFrameLength(const uint8_t *data, size_t bytes) {
    uint16_t len;
    if (bytes < 7) return -1;
    if (0xFF != data[0] || 0xF0 != (data[1] & 0xF0)) {
        return -1;
    }
    len = ((uint16_t) (data[3] & 0x03) << 11) | ((uint16_t) data[4] << 3) | ((uint16_t) (data[5] >> 5) & 0x07);
    return len;
}

string makeAacConfig(const uint8_t *hex, size_t length){
#ifndef ENABLE_MP4
    if (!(hex[0] == 0xFF && (hex[1] & 0xF0) == 0xF0)) {
        return "";
    }
    // Get and check the 'profile':
    unsigned char profile = (hex[2] & 0xC0) >> 6; // 2 bits
    if (profile == 3) {
        return "";
    }

    // Get and check the 'sampling_frequency_index':
    unsigned char sampling_frequency_index = (hex[2] & 0x3C) >> 2; // 4 bits
    if (samplingFrequencyTable[sampling_frequency_index] == 0) {
        return "";
    }

    // Get and check the 'channel_configuration':
    unsigned char channel_configuration = ((hex[2] & 0x01) << 2) | ((hex[3] & 0xC0) >> 6); // 3 bits
    unsigned char audioSpecificConfig[2];
    unsigned char const audioObjectType = profile + 1;
    audioSpecificConfig[0] = (audioObjectType << 3) | (sampling_frequency_index >> 1);
    audioSpecificConfig[1] = (sampling_frequency_index << 7) | (channel_configuration << 3);
    return string((char *)audioSpecificConfig,2);
#else
    struct mpeg4_aac_t aac;
    memset(&aac, 0, sizeof(aac));
    if (mpeg4_aac_adts_load(hex, length, &aac) > 0) {
        char buf[32] = {0};
        int len = mpeg4_aac_audio_specific_config_save(&aac, (uint8_t *) buf, sizeof(buf));
        if (len > 0) {
            return string(buf, len);
        }
    }
    WarnL << "生成aac config失败, adts header:" << hexdump(hex, length);
    return "";
#endif
}

int dumpAacConfig(const string &config, size_t length, uint8_t *out, size_t out_size) {
#ifndef ENABLE_MP4
    AdtsHeader header;
    parseAacConfig(config, header);
    header.aac_frame_length = (decltype(header.aac_frame_length))(ADTS_HEADER_LEN + length);
    dumpAdtsHeader(header, out);
    return ADTS_HEADER_LEN;
#else
    struct mpeg4_aac_t aac;
    memset(&aac, 0, sizeof(aac));
    int ret = mpeg4_aac_audio_specific_config_load((uint8_t *) config.data(), config.size(), &aac);
    if (ret > 0) {
        ret = mpeg4_aac_adts_save(&aac, length, out, out_size);
    }
    if (ret < 0) {
        WarnL << "生成adts头失败:" << ret << ", aac config:" << hexdump(config.data(), config.size());
    }
    assert((int)out_size >= ret);
    return ret;
#endif
}

bool parseAacConfig(const string &config, int &samplerate, int &channels){
#ifndef ENABLE_MP4
    AdtsHeader header;
    parseAacConfig(config, header);
    samplerate = samplingFrequencyTable[header.sf_index];
    channels = header.channel_configuration;
    return true;
#else
    struct mpeg4_aac_t aac;
    memset(&aac, 0, sizeof(aac));
    int ret = mpeg4_aac_audio_specific_config_load((uint8_t *) config.data(), config.size(), &aac);
    if (ret > 0) {
        samplerate = aac.sampling_frequency;
        channels = aac.channels;
        return true;
    }
    WarnL << "获取aac采样率、声道数失败:" << hexdump(config.data(), config.size());
    return false;
#endif
}

////////////////////////////////////////////////////////////////////////////////////////////////////

/**
 * aac类型SDP
 */
class AACSdp : public Sdp {
public:
    /**
     * 构造函数
     * @param aac_cfg aac两个字节的配置描述
     * @param sample_rate 音频采样率
     * @param payload_type rtp payload type 默认98
     * @param bitrate 比特率
     */
    AACSdp(const string &aac_cfg,
           int sample_rate,
           int channels,
           int bitrate = 128,
           int payload_type = 98) : Sdp(sample_rate,payload_type){
        _printer << "m=audio 0 RTP/AVP " << payload_type << "\r\n";
        if (bitrate) {
            _printer << "b=AS:" << bitrate << "\r\n";
        }
        _printer << "a=rtpmap:" << payload_type << " " << getCodecName() << "/" << sample_rate << "/" << channels << "\r\n";

        string configStr;
        char buf[4] = {0};
        for(auto &ch : aac_cfg){
            snprintf(buf, sizeof(buf), "%02X", (uint8_t)ch);
            configStr.append(buf);
        }
        _printer << "a=fmtp:" << payload_type << " streamtype=5;profile-level-id=1;mode=AAC-hbr;"
                 << "sizelength=13;indexlength=3;indexdeltalength=3;config=" << configStr << "\r\n";
        _printer << "a=control:trackID=" << (int)TrackAudio << "\r\n";
    }

    string getSdp() const override {
        return _printer;
    }

    CodecId getCodecId() const override {
        return CodecAAC;
    }
private:
    _StrPrinter _printer;
};

////////////////////////////////////////////////////////////////////////////////////////////////////

AACTrack::AACTrack(const string &aac_cfg) {
    if (aac_cfg.size() < 2) {
        throw std::invalid_argument("adts配置必须最少2个字节");
    }
    _cfg = aac_cfg;
    onReady();
}

const string &AACTrack::getAacCfg() const {
    return _cfg;
}

CodecId AACTrack::getCodecId() const {
    return CodecAAC;
}

bool AACTrack::ready() {
    return !_cfg.empty();
}

int AACTrack::getAudioSampleRate() const {
    return _sampleRate;
}

int AACTrack::getAudioSampleBit() const {
    return _sampleBit;
}

int AACTrack::getAudioChannel() const {
    return _channel;
}

bool AACTrack::inputFrame(const Frame::Ptr &frame) {
    if (!frame->prefixSize()) {
        return inputFrame_l(frame);
    }

    bool ret = false;
    //有adts头，尝试分帧
    int64_t dts = frame->dts();
    int64_t pts = frame->pts();

    auto ptr = frame->data();
    auto end = frame->data() + frame->size();
    while (ptr < end) {
        auto frame_len = getAacFrameLength((uint8_t *) ptr, end - ptr);
        if (frame_len < ADTS_HEADER_LEN) {
            break;
        }
        if (frame_len == (int)frame->size()) {
            return inputFrame_l(frame);
        }
        auto sub_frame = std::make_shared<FrameTSInternal<FrameFromPtr> >(frame, (char *) ptr, frame_len, ADTS_HEADER_LEN,dts,pts);
        ptr += frame_len;
        if (ptr > end) {
            WarnL << "invalid aac length in adts header: " << frame_len
                  << ", remain data size: " << end - (ptr - frame_len);
            break;
        }
        sub_frame->setCodecId(CodecAAC);
        if (inputFrame_l(sub_frame)) {
            ret = true;
        }
        dts += 1024*1000/getAudioSampleRate();
        pts += 1024*1000/getAudioSampleRate();
    }
    return ret;
}

bool AACTrack::inputFrame_l(const Frame::Ptr &frame) {
    if (_cfg.empty()) {
        //未获取到aac_cfg信息
        if (frame->prefixSize()) {
            //根据7个字节的adts头生成aac config
            _cfg = makeAacConfig((uint8_t *) (frame->data()), frame->prefixSize());
            onReady();
        } else {
            WarnL << "无法获取adts头!";
        }
    }

    if (frame->size() > frame->prefixSize()) {
        //除adts头外，有实际负载
        return AudioTrack::inputFrame(frame);
    }
    return false;
}

void AACTrack::onReady() {
    if (_cfg.size() < 2) {
        return;
    }
    parseAacConfig(_cfg, _sampleRate, _channel);
}

Track::Ptr AACTrack::clone() {
    return std::make_shared<std::remove_reference<decltype(*this)>::type>(*this);
}

Sdp::Ptr AACTrack::getSdp() {
    if(!ready()){
        WarnL << getCodecName() << " Track未准备好";
        return nullptr;
    }
    return std::make_shared<AACSdp>(getAacCfg(), getAudioSampleRate(), getAudioChannel(), getBitRate() / 1024);
}

}//namespace mediakit