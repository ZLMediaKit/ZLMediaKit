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
    G711Track(CodecId codecId){
        _codecid = codecId;
    }

    /**
     * 返回编码类型
     */
    CodecId getCodecId() const override{
        return _codecid;
    }

    /**
     * G711的Track不需要初始化
     */
    bool ready() override {
        return true;
    }

    /**
     * 返回音频采样率
     */
    int getAudioSampleRate() const override{
        return 8000;
    }

    /**
     * 返回音频采样位数，一般为16或8
     */
    int getAudioSampleBit() const override{
        return 16;
    }

    /**
     * 返回音频通道数
     */
    int getAudioChannel() const override{
        return 1;
    }

private:
    Track::Ptr clone() override {
        return std::make_shared<std::remove_reference<decltype(*this)>::type >(*this);
    }

    //生成sdp
    Sdp::Ptr getSdp() override ;
private:
    CodecId _codecid = CodecG711A;
};

/**
 * G711类型SDP
 */
class G711Sdp : public Sdp {
public:
    /**
     * G711采样率固定为8000
     * @param codecId G711A G711U
     */
    G711Sdp(CodecId codecId) : Sdp(8000,payloadType(codecId)), _codecId(codecId){
        int pt = payloadType(codecId);
        _printer << "m=audio 0 RTP/AVP " << pt << "\r\n";
        _printer << "a=rtpmap:" << pt << (codecId == CodecG711A ? " PCMA/" : " PCMU/") << 8000 << "\r\n";
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

    int payloadType(CodecId codecId){
        switch (codecId){
            case CodecG711A : return 8;
            case CodecG711U : return 0;
            default : return  -1;
        }
    }

private:
    _StrPrinter _printer;
    CodecId _codecId;
};

}//namespace mediakit


#endif //ZLMEDIAKIT_AAC_H
