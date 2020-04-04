/*
 * Copyright (c) 2016 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/xiongziliang/ZLMediaKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#ifndef ZLMEDIAKIT_AAC_H
#define ZLMEDIAKIT_AAC_H

#include "Frame.h"
#include "Track.h"

namespace mediakit{

class AACFrame;

unsigned const samplingFrequencyTable[16] = { 96000, 88200,
                                              64000, 48000,
                                              44100, 32000,
                                              24000, 22050,
                                              16000, 12000,
                                              11025, 8000,
                                              7350, 0, 0, 0 };

void	makeAdtsHeader(const string &strAudioCfg,AACFrame &adts);
void 	writeAdtsHeader(const AACFrame &adts, uint8_t *pcAdts) ;
string 	makeAdtsConfig(const uint8_t *pcAdts);
void 	getAACInfo(const AACFrame &adts,int &iSampleRate,int &iChannel);


/**
 * aac帧，包含adts头
 */
class AACFrame : public Frame {
public:
    typedef std::shared_ptr<AACFrame> Ptr;

    char *data() const override{
        return (char *)buffer;
    }
    uint32_t size() const override {
        return aac_frame_length;
    }
    uint32_t dts() const override {
        return timeStamp;
    }
    uint32_t prefixSize() const override{
        return iPrefixSize;
    }

    TrackType getTrackType() const override{
        return TrackAudio;
    }

    CodecId getCodecId() const override{
        return CodecAAC;
    }

    bool keyFrame() const override {
        return false;
    }

    bool configFrame() const override{
        return false;
    }
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
    unsigned char buffer[2 * 1024 + 7];
    uint32_t timeStamp;
    uint32_t iPrefixSize = 7;
} ;

class AACFrameNoCacheAble : public FrameNoCacheAble {
public:
    typedef std::shared_ptr<AACFrameNoCacheAble> Ptr;

    AACFrameNoCacheAble(char *ptr,uint32_t size,uint32_t dts,uint32_t pts = 0,int prefixeSize = 7){
        _ptr = ptr;
        _size = size;
        _dts = dts;
        _prefixSize = prefixeSize;
    }

    TrackType getTrackType() const override{
        return TrackAudio;
    }

    CodecId getCodecId() const override{
        return CodecAAC;
    }

    bool keyFrame() const override {
        return false;
    }

    bool configFrame() const override{
        return false;
    }
} ;


/**
 * aac音频通道
 */
class AACTrack : public AudioTrack{
public:
    typedef std::shared_ptr<AACTrack> Ptr;

    /**
     * 延后获取adts头信息
     * 在随后的inputFrame中获取adts头信息
     */
    AACTrack(){}

    /**
     * 构造aac类型的媒体
     * @param aac_cfg aac两个字节的配置信息
     */
    AACTrack(const string &aac_cfg){
        if(aac_cfg.size() < 2){
            throw std::invalid_argument("adts配置必须最少2个字节");
        }
        _cfg = aac_cfg.substr(0,2);
        onReady();
    }

    /**
     * 构造aac类型的媒体
     * @param adts_header adts头，7个字节
     * @param adts_header_len adts头长度，不少于7个字节
     */
    AACTrack(const char *adts_header,int adts_header_len = 7){
        if(adts_header_len < 7){
            throw std::invalid_argument("adts头必须不少于7个字节");
        }
        _cfg = makeAdtsConfig((uint8_t*)adts_header);
        onReady();
    }

    /**
     * 构造aac类型的媒体
     * @param aac_frame_with_adts 带adts头的aac帧
     */
    AACTrack(const Frame::Ptr &aac_frame_with_adts){
        if(aac_frame_with_adts->getCodecId() != CodecAAC || aac_frame_with_adts->prefixSize() < 7){
            throw std::invalid_argument("必须输入带adts头的aac帧");
        }
        _cfg = makeAdtsConfig((uint8_t*)aac_frame_with_adts->data());
        onReady();
    }

    /**
     * 获取aac两个字节的配置
     * @return
     */
    const string &getAacCfg() const{
        return _cfg;
    }

    /**
     * 返回编码类型
     * @return
     */
    CodecId getCodecId() const override{
        return CodecAAC;
    }

    /**
     * 在获取aac_cfg前是无效的Track
     * @return
     */
    bool ready() override {
        return !_cfg.empty();
    }


    /**
    * 返回音频采样率
    * @return
    */
    int getAudioSampleRate() const override{
        return _sampleRate;
    }
    /**
     * 返回音频采样位数，一般为16或8
     * @return
     */
    int getAudioSampleBit() const override{
        return _sampleBit;
    }
    /**
     * 返回音频通道数
     * @return
     */
    int getAudioChannel() const override{
        return _channel;
    }

    /**
    * 输入数据帧,并获取aac_cfg
    * @param frame 数据帧
    */
    void inputFrame(const Frame::Ptr &frame) override{
        if(_cfg.empty()){
            //未获取到aac_cfg信息
            if(frame->prefixSize() >= 7) {
                //7个字节的adts头
                _cfg = makeAdtsConfig(reinterpret_cast<const uint8_t *>(frame->data()));
                onReady();
            }else{
                WarnL << "无法获取adts头!";
            }
        }
        AudioTrack::inputFrame(frame);
    }
private:
    /**
     * 解析2个字节的aac配置
     */
    void onReady(){
        if(_cfg.size() < 2){
            return;
        }
        AACFrame aacFrame;
        makeAdtsHeader(_cfg,aacFrame);
        getAACInfo(aacFrame,_sampleRate,_channel);
    }
    Track::Ptr clone() override {
        return std::make_shared<std::remove_reference<decltype(*this)>::type >(*this);
    }

    //生成sdp
    Sdp::Ptr getSdp() override ;
private:
    string _cfg;
    int _sampleRate = 0;
    int _sampleBit = 16;
    int _channel = 0;
};


/**
* aac类型SDP
*/
class AACSdp : public Sdp {
public:

    /**
     *
     * @param aac_cfg aac两个字节的配置描述
     * @param sample_rate 音频采样率
     * @param playload_type rtp playload type 默认98
     * @param bitrate 比特率
     */
    AACSdp(const string &aac_cfg,
           int sample_rate,
           int playload_type = 98,
           int bitrate = 128) : Sdp(sample_rate,playload_type){
        _printer << "m=audio 0 RTP/AVP " << playload_type << "\r\n";
        _printer << "b=AS:" << bitrate << "\r\n";
        _printer << "a=rtpmap:" << playload_type << " MPEG4-GENERIC/" << sample_rate << "\r\n";

        char configStr[32] = {0};
        snprintf(configStr, sizeof(configStr), "%02X%02X", (uint8_t)aac_cfg[0], (uint8_t)aac_cfg[1]);
        _printer << "a=fmtp:" << playload_type << " streamtype=5;profile-level-id=1;mode=AAC-hbr;"
                 << "sizelength=13;indexlength=3;indexdeltalength=3;config="
                 << configStr << "\r\n";
        _printer << "a=control:trackID=" << getTrackType() << "\r\n";
    }

    string getSdp() const override {
        return _printer;
    }

    TrackType getTrackType() const override {
        return TrackAudio;
    }
    CodecId getCodecId() const override {
        return CodecAAC;
    }
private:
    _StrPrinter _printer;
};

}//namespace mediakit


#endif //ZLMEDIAKIT_AAC_H
