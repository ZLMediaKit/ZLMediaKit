/*
 * Copyright (c) 2016 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/xiongziliang/ZLMediaKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#ifndef ZLMEDIAKIT_OPUS_H
#define ZLMEDIAKIT_OPUS_H

#include "Frame.h"
#include "Track.h"

namespace mediakit{

/**
 * Opus帧
 */
class OpusFrame : public FrameImp {
public:
    typedef std::shared_ptr<OpusFrame> Ptr;

    OpusFrame(){
        _codecid = CodecOpus;
    }
};

/**
 * 不可缓存的Opus帧
 */
class OpusFrameNoCacheAble : public FrameFromPtr {
public:
    typedef std::shared_ptr<OpusFrameNoCacheAble> Ptr;

    OpusFrameNoCacheAble(char *ptr,uint32_t size,uint32_t dts, uint32_t pts = 0,int prefix_size = 0){
        _ptr = ptr;
        _size = size;
        _dts = dts;
        _prefix_size = prefix_size;
    }

    CodecId getCodecId() const override{
        return CodecOpus;
    }

    bool keyFrame() const override {
        return false;
    }

    bool configFrame() const override{
        return false;
    }
};

/**
 * Opus帧音频通道
 */
class OpusTrack : public AudioTrackImp{
public:
    typedef std::shared_ptr<OpusTrack> Ptr;
    OpusTrack(int sample_rate, int channels, int sample_bit) : AudioTrackImp(CodecOpus,sample_rate,channels,sample_bit){}

private:
    //克隆该Track
    Track::Ptr clone() override {
        return std::make_shared<std::remove_reference<decltype(*this)>::type >(*this);
    }
    //生成sdp
    Sdp::Ptr getSdp() override ;
};

/**
 * Opus类型SDP
 */
class OpusSdp : public Sdp {
public:
    /**
     * 构造opus sdp
     * @param sample_rate 音频采样率
     * @param payload_type rtp payload
     * @param bitrate 比特率
     */
    OpusSdp(int sample_rate,
            int channels,
            int payload_type = 98,
            int bitrate = 128) : Sdp(sample_rate,payload_type){
        _printer << "m=audio 0 RTP/AVP " << payload_type << "\r\n";
        _printer << "a=rtpmap:" << payload_type << " opus/" << sample_rate  << "/" << channels << "\r\n";
        _printer << "a=control:trackID=" << (int)TrackAudio << "\r\n";
    }

    string getSdp() const override {
        return _printer;
    }

    CodecId getCodecId() const override {
        return CodecOpus;
    }
private:
    _StrPrinter _printer;
};

}//namespace mediakit
#endif //ZLMEDIAKIT_OPUS_H
