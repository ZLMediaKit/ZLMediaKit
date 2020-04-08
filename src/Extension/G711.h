/*
 * Copyright (c) 2016 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/xiongziliang/ZLMediaKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#ifndef ZLMEDIAKIT_G711_H
#define ZLMEDIAKIT_G711_H

#include "Frame.h"
#include "Track.h"

namespace mediakit{

class G711Frame;

unsigned const samplingFrequencyTableG711[16] = { 96000, 88200,
                                              64000, 48000,
                                              44100, 32000,
                                              24000, 22050,
                                              16000, 12000,
                                              11025, 8000,
                                              7350, 0, 0, 0 };

void	makeAdtsHeader(const string &strAudioCfg,G711Frame &adts);
void 	writeAdtsHeader(const G711Frame &adts, uint8_t *pcAdts) ;
string 	makeG711AdtsConfig(const uint8_t *pcAdts);
void 	getAACInfo(const G711Frame &adts,int &iSampleRate,int &iChannel);


/**
 * aac帧，包含adts头
 */
class G711Frame : public Frame {
public:
    typedef std::shared_ptr<G711Frame> Ptr;

    char *data() const override{
        return (char *)buffer;
    }
    uint32_t size() const override {
        return frameLength;
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
    unsigned int frameLength; // 一个帧的长度包括 raw data block
    unsigned char buffer[2 * 1024 + 7];
    uint32_t timeStamp;
    uint32_t iPrefixSize = 0;
} ;

class G711FrameNoCacheAble : public FrameNoCacheAble {
public:
    typedef std::shared_ptr<G711FrameNoCacheAble> Ptr;

    G711FrameNoCacheAble(CodecId codecId, char *ptr,uint32_t size,uint32_t dts,int prefixeSize = 7){
        _codecId = codecId;
        _ptr = ptr;
        _size = size;
        _dts = dts;
        _prefixSize = prefixeSize;
    }

    TrackType getTrackType() const override{
        return TrackAudio;
    }

    CodecId getCodecId() const override{
        return _codecId;
    }

    bool keyFrame() const override {
        return false;
    }

    bool configFrame() const override{
        return false;
    }

private:
    CodecId _codecId;
} ;


/**
 * g711音频通道
 */
class G711Track : public AudioTrack{
public:
    typedef std::shared_ptr<G711Track> Ptr;

    /**
     * 延后获取adts头信息
     * 在随后的inputFrame中获取adts头信息
     */
    G711Track(){}

    /**
     * G711A G711U
     */
    G711Track(CodecId codecId, int sampleBit = 16, int sampleRate = 8000){
        _codecid = codecId;
        _sampleBit = sampleBit;
        _sampleRate = sampleRate;
        onReady();
    }

    /**
     * 返回编码类型
     * @return
     */
    CodecId getCodecId() const override{
        return _codecid;
    }

    /**
     * 在获取aac_cfg前是无效的Track
     * @return
     */
    bool ready() override {
        return true;
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
        AudioTrack::inputFrame(frame);
    }
private:
    /**
     * 
     */
    void onReady(){
/*
        if(_cfg.size() < 2){
            return;
        }
        G711Frame aacFrame;
        makeAdtsHeader(_cfg,aacFrame);
        getAACInfo(aacFrame,_sampleRate,_channel);*/
    }
    Track::Ptr clone() override {
        return std::make_shared<std::remove_reference<decltype(*this)>::type >(*this);
    }

    //生成sdp
    Sdp::Ptr getSdp() override ;
private:
    string _cfg;
    CodecId _codecid = CodecG711A;
    int _sampleRate = 8000;
    int _sampleBit = 16;
    int _channel = 1;
};


/**
* aac类型SDP
*/
class G711Sdp : public Sdp {
public:

    /**
     *
     * @param aac_codecId G711A G711U
     * @param sample_rate 音频采样率
     * @param playload_type rtp playload type 默认0为G711U， 8为G711A
     * @param bitrate 比特率
     */
    G711Sdp(CodecId codecId,
           int sample_rate,
           int playload_type = 0,
           int bitrate = 128) : Sdp(sample_rate,playload_type), _codecId(codecId){
        _printer << "m=audio 0 RTP/AVP " << playload_type << "\r\n";
        //_printer << "b=AS:" << bitrate << "\r\n";
        _printer << "a=rtpmap:" << playload_type << (codecId == CodecG711A ? " PCMA/" : " PCMU/") << sample_rate << "\r\n";
        _printer << "a=control:trackID=" << getTrackType() << "\r\n";
    }

    string getSdp() const override {
        return _printer;
    }

    TrackType getTrackType() const override {
        return TrackAudio;
    }
    CodecId getCodecId() const override {
        return _codecId;
    }
private:
    _StrPrinter _printer;
    CodecId _codecId = CodecG711A;
};

}//namespace mediakit


#endif //ZLMEDIAKIT_AAC_H
