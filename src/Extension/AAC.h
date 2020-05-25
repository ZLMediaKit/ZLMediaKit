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
#define ADTS_HEADER_LEN 7

namespace mediakit{

string makeAacConfig(const uint8_t *hex);
void dumpAacConfig(const string &config, int length, uint8_t *out);
void parseAacConfig(const string &config, int &samplerate, int &channels);

/**
 * aac帧，包含adts头
 */
class AACFrame : public FrameImp {
public:
    typedef std::shared_ptr<AACFrame> Ptr;
    AACFrame(){
        _codecid = CodecAAC;
    }
};

class AACFrameNoCacheAble : public FrameFromPtr {
public:
    typedef std::shared_ptr<AACFrameNoCacheAble> Ptr;

    AACFrameNoCacheAble(char *ptr,uint32_t size,uint32_t dts,uint32_t pts = 0,int prefix_size = ADTS_HEADER_LEN){
        _ptr = ptr;
        _size = size;
        _dts = dts;
        _prefix_size = prefix_size;
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
};

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
     * 获取aac两个字节的配置
     */
    const string &getAacCfg() const{
        return _cfg;
    }

    /**
     * 返回编码类型
     */
    CodecId getCodecId() const override{
        return CodecAAC;
    }

    /**
     * 在获取aac_cfg前是无效的Track
     */
    bool ready() override {
        return !_cfg.empty();
    }

    /**
     * 返回音频采样率
     */
    int getAudioSampleRate() const override{
        return _sampleRate;
    }

    /**
     * 返回音频采样位数，一般为16或8
     */
    int getAudioSampleBit() const override{
        return _sampleBit;
    }

    /**
     * 返回音频通道数
     */
    int getAudioChannel() const override{
        return _channel;
    }

    /**
     * 输入数据帧,并获取aac_cfg
     * @param frame 数据帧
     */
    void inputFrame(const Frame::Ptr &frame) override{
        if (_cfg.empty()) {
            //未获取到aac_cfg信息
            if (frame->prefixSize() >= ADTS_HEADER_LEN) {
                //7个字节的adts头
                _cfg = makeAacConfig((uint8_t *) (frame->data()));
                onReady();
            } else {
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
        if (_cfg.size() < 2) {
            return;
        }
        parseAacConfig(_cfg, _sampleRate, _channel);
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
     * 构造函数
     * @param aac_cfg aac两个字节的配置描述
     * @param sample_rate 音频采样率
     * @param payload_type rtp payload type 默认98
     * @param bitrate 比特率
     */
    AACSdp(const string &aac_cfg,
           int sample_rate,
           int channels,
           int payload_type = 98,
           int bitrate = 128) : Sdp(sample_rate,payload_type){
        _printer << "m=audio 0 RTP/AVP " << payload_type << "\r\n";
        _printer << "b=AS:" << bitrate << "\r\n";
        _printer << "a=rtpmap:" << payload_type << " MPEG4-GENERIC/" << sample_rate << "/" << channels << "\r\n";

        char configStr[32] = {0};
        snprintf(configStr, sizeof(configStr), "%02X%02X", (uint8_t)aac_cfg[0], (uint8_t)aac_cfg[1]);
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

}//namespace mediakit
#endif //ZLMEDIAKIT_AAC_H