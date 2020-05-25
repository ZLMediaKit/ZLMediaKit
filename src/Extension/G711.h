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
class G711Frame : public FrameImp {
public:
    G711Frame(){
        _codecid = CodecG711A;
    }
};

class G711FrameNoCacheAble : public FrameFromPtr {
public:
    typedef std::shared_ptr<G711FrameNoCacheAble> Ptr;

    G711FrameNoCacheAble(char *ptr,uint32_t size,uint32_t dts, uint32_t pts = 0,int prefix_size = 0){
        _ptr = ptr;
        _size = size;
        _dts = dts;
        _prefix_size = prefix_size;
    }

    void setCodec(CodecId codecId){
        _codecId = codecId;
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
class G711Track : public AudioTrackImp{
public:
    typedef std::shared_ptr<G711Track> Ptr;
    G711Track(CodecId codecId,int sample_rate, int channels, int sample_bit) : AudioTrackImp(codecId,sample_rate,channels,sample_bit){}

private:
    //克隆该Track
    Track::Ptr clone() override {
        return std::make_shared<std::remove_reference<decltype(*this)>::type >(*this);
    }
    //生成sdp
    Sdp::Ptr getSdp() override ;
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
     * @param payload_type rtp payload
     * @param bitrate 比特率
     */
    G711Sdp(CodecId codecId,
            int sample_rate,
            int channels,
            int payload_type = 98,
            int bitrate = 128) : Sdp(sample_rate,payload_type), _codecId(codecId){
        _printer << "m=audio 0 RTP/AVP " << payload_type << "\r\n";
        _printer << "a=rtpmap:" << payload_type << (codecId == CodecG711A ? " PCMA/" : " PCMU/") << sample_rate  << "/" << channels << "\r\n";
        _printer << "a=control:trackID=" << (int)TrackAudio << "\r\n";
    }

    string getSdp() const override {
        return _printer;
    }

    CodecId getCodecId() const override {
        return _codecId;
    }
private:
    _StrPrinter _printer;
    CodecId _codecId;
};

}//namespace mediakit
#endif //ZLMEDIAKIT_G711_H