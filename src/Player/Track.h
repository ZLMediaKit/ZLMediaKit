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

class TrackFormat : public FrameRingInterface , public CodecInfo{
public:
    typedef std::shared_ptr<TrackFormat> Ptr;
    TrackFormat(){}
    virtual ~TrackFormat(){}
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
    CodecId getCodecId() const override{
        return CodecH264;
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
    CodecId getCodecId() const override{
        return CodecAAC;
    }
private:
    string _cfg;
};


#endif //ZLMEDIAKIT_TRACK_H
