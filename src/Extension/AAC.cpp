/*
 * Copyright (c) 2016 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/xiongziliang/ZLMediaKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#include "AAC.h"

namespace mediakit{

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

string 	makeAacConfig(const uint8_t *hex){
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
}

void dumpAacConfig(const string &config, int length, uint8_t *out){
    AdtsHeader header;
    parseAacConfig(config, header);
    header.aac_frame_length = length;
    dumpAdtsHeader(header, out);
}

void parseAacConfig(const string &config, int &samplerate, int &channels){
    AdtsHeader header;
    parseAacConfig(config, header);
    samplerate = samplingFrequencyTable[header.sf_index];
    channels = header.channel_configuration;
}

Sdp::Ptr AACTrack::getSdp() {
    if(!ready()){
        WarnL << getCodecName() << " Track未准备好";
        return nullptr;
    }
    return std::make_shared<AACSdp>(getAacCfg(),getAudioSampleRate(), getAudioChannel());
}

}//namespace mediakit