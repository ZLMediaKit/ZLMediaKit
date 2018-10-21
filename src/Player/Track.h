//
// Created by xzl on 2018/10/21.
//

#ifndef ZLMEDIAKIT_TRACK_H
#define ZLMEDIAKIT_TRACK_H

#include <memory>
#include <string>
#include "Frame.h"
#include "Util/RingBuffer.h"
#include "Rtsp/Rtsp.h"

using namespace std;
using namespace ZL::Util;

class TrackFormat {
public:
    typedef std::shared_ptr<TrackFormat> Ptr;
    typedef RingBuffer<Frame::Ptr> RingType;

    typedef enum {
        CodecInvalid = -1,
        CodecH264 = 0,
        CodecAAC = 0x0100,
        CodecMax
    } CodecID;

    TrackFormat(){
        _ring = std::make_shared<RingType>();
    }
    virtual ~TrackFormat(){}
    virtual TrackType getTrackType() const = 0;
    virtual int getCodecId() const = 0;


    void writeFrame(const Frame::Ptr &frame,bool keypos = true){
        _ring->write(frame, keypos);
    }

    RingType::Ptr& getRing() {
        return _ring;
    }
private:
    RingType::Ptr _ring;
};

class VideoTrackFormat : public TrackFormat {
public:
    TrackType getTrackType() const override { return TrackVideo;};
    virtual int getVideoHeight() const = 0;
    virtual int getVideoWidth() const  = 0;
    virtual float getVideoFps() const = 0;
};

class AudioTrackFormat : public TrackFormat {
public:
    TrackType getTrackType() const override { return TrackAudio;};
    virtual int getAudioSampleRate() const  = 0;
    virtual int getAudioSampleBit() const = 0;
    virtual int getAudioChannel() const = 0;
};

class H264TrackFormat : public VideoTrackFormat{
public:
    H264TrackFormat(const string &sps,const string &pps){
        _sps = sps;
        _pps = pps;
    }
    const string &getSps() const{
        return _sps;
    }
    const string &getPps() const{
        return _pps;
    }
    int getCodecId() const override{
        return TrackFormat::CodecH264;
    }
private:
    string _sps;
    string _pps;
};

class AACTrackFormat : public AudioTrackFormat{
public:
    AACTrackFormat(const string &aac_cfg){
        _cfg = aac_cfg;
    }
    const string &getAacCfg() const{
        return _cfg;
    }
    int getCodecId() const override{
        return TrackFormat::CodecAAC;
    }
private:
    string _cfg;
};


#endif //ZLMEDIAKIT_TRACK_H
