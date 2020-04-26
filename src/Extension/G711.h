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

/**
 * G711帧
 */
class G711Frame : public Frame {
public:
    typedef std::shared_ptr<G711Frame> Ptr;

    char *data() const override{
        return (char *)buffer.data();
    }

    uint32_t size() const override {
        return buffer.size();
    }

    uint32_t dts() const override {
        return timeStamp;
    }

    uint32_t prefixSize() const override{
        return 0;
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
public:
    CodecId _codecId = CodecG711A;
    string buffer;
    uint32_t timeStamp;
} ;

class G711FrameNoCacheAble : public FrameNoCacheAble {
public:
    typedef std::shared_ptr<G711FrameNoCacheAble> Ptr;

    //兼容通用接口
    G711FrameNoCacheAble(char *ptr,uint32_t size,uint32_t dts, uint32_t pts = 0,int prefixeSize = 0){
        _ptr = ptr;
        _size = size;
        _dts = dts;
        _prefixSize = prefixeSize;
    }

    //兼容通用接口
    void setCodec(CodecId codecId){
        _codecId = codecId;
    }

    G711FrameNoCacheAble(CodecId codecId, char *ptr,uint32_t size,uint32_t dts,int prefixeSize = 0){
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
};

/**
 * G711音频通道
 */
class G711Track : public AudioTrack{
public:
    typedef std::shared_ptr<G711Track> Ptr;

    /**
     * G711A G711U
     */
    G711Track(CodecId codecId,int sample_rate, int channels, int sample_bit){
        _codecid = codecId;
        _sample_rate = sample_rate;
        _channels = channels;
        _sample_bit = sample_bit;
    }

    /**
     * 返回编码类型
     */
    CodecId getCodecId() const override{
        return _codecid;
    }

    /**
     * 是否已经初始化
     */
    bool ready() override {
        return true;
    }

    /**
     * 返回音频采样率
     */
    int getAudioSampleRate() const override{
        return _sample_rate;
    }

    /**
     * 返回音频采样位数，一般为16或8
     */
    int getAudioSampleBit() const override{
        return _sample_bit;
    }

    /**
     * 返回音频通道数
     */
    int getAudioChannel() const override{
        return _channels;
    }

private:
    Track::Ptr clone() override {
        return std::make_shared<std::remove_reference<decltype(*this)>::type >(*this);
    }

    //生成sdp
    Sdp::Ptr getSdp() override ;
private:
    CodecId _codecid;
    int _sample_rate;
    int _channels;
    int _sample_bit;
};

/**
 * G711类型SDP
 */
class G711Sdp : public Sdp {
public:
    /**
     * G711采样率固定为8000
     * @param codecId G711A G711U
     * @param sample_rate 音频采样率
     * @param playload_type rtp playload
     * @param bitrate 比特率
     */
    G711Sdp(CodecId codecId,
            int sample_rate,
            int channels,
            int playload_type = 98,
            int bitrate = 128) : Sdp(sample_rate,playload_type), _codecId(codecId){
        _printer << "m=audio 0 RTP/AVP " << playload_type << "\r\n";
        _printer << "a=rtpmap:" << playload_type << (codecId == CodecG711A ? " PCMA/" : " PCMU/") << sample_rate  << "/" << channels << "\r\n";
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
    CodecId _codecId;
};

}//namespace mediakit


#endif //ZLMEDIAKIT_AAC_H
