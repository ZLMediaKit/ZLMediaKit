/*
 * MIT License
 *
 * Copyright (c) 2016 xiongziliang <771730766@qq.com>
 *
 * This file is part of ZLMediaKit(https://github.com/xiongziliang/ZLMediaKit).
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#ifndef ZLMEDIAKIT_RTMPMETEDATA_H
#define ZLMEDIAKIT_RTMPMETEDATA_H

#include "RtmpMuxer/H264RtmpCodec.h"
#include "RtmpMuxer/AACRtmpCodec.h"
#include "Util/base64.h"
#include "Extension/Track.h"
#include "Rtmp/amf.h"
#include "Extension/Factory.h"

namespace mediakit {

/**
 * rtmp metedata基类，用于描述rtmp格式信息
 */
class Metedata : public CodecInfo{
public:
    typedef std::shared_ptr<Metedata> Ptr;

    Metedata():_metedata(AMF_OBJECT){}
    virtual ~Metedata(){}
    const AMFValue &getMetedata() const{
        return _metedata;
    }
protected:
    AMFValue _metedata;
};

/**
* metedata中除音视频外的其他描述部分
*/
class TitleMete : public Metedata{
public:
    typedef std::shared_ptr<TitleMete> Ptr;

    TitleMete(float dur_sec = 0,
              uint64_t fileSize = 0,
              const map<string,string> &header = map<string,string>()){
        _metedata.set("duration", dur_sec);
        _metedata.set("fileSize", 0);
        _metedata.set("server","ZLMediaKit");
        for (auto &pr : header){
            _metedata.set(pr.first, pr.second);
        }
    }

    /**
     * 返回音频或视频类型
     * @return
     */
    TrackType getTrackType() const override {
        return TrackTitle;
    }

    /**
     * 返回编码器id
     * @return
     */
    CodecId getCodecId() const override{
        return CodecInvalid;
    }
};

class VideoMete : public Metedata{
public:
    typedef std::shared_ptr<VideoMete> Ptr;

    VideoMete(const VideoTrack::Ptr &video,int datarate = 5000){
        if(video->getVideoWidth() > 0 ){
            _metedata.set("width", video->getVideoWidth());
        }
        if(video->getVideoHeight() > 0 ){
            _metedata.set("height", video->getVideoHeight());
        }
        if(video->getVideoFps() > 0 ){
            _metedata.set("framerate", video->getVideoFps());
        }
        _metedata.set("videodatarate", datarate);
        _codecId = video->getCodecId();
        _metedata.set("videocodecid", Factory::getAmfByCodecId(_codecId));
    }
    virtual ~VideoMete(){}

    /**
     * 返回音频或视频类型
     * @return
     */
    TrackType getTrackType() const override {
        return TrackVideo;
    }

    /**
     * 返回编码器id
     * @return
     */
    CodecId getCodecId() const override{
        return _codecId;
    }
private:
    CodecId _codecId;
};


class AudioMete : public Metedata{
public:
    typedef std::shared_ptr<AudioMete> Ptr;

    AudioMete(const AudioTrack::Ptr &audio,int datarate = 160){
        _metedata.set("audiodatarate", datarate);
        if(audio->getAudioSampleRate() > 0){
            _metedata.set("audiosamplerate", audio->getAudioSampleRate());
        }
        if(audio->getAudioSampleBit() > 0){
            _metedata.set("audiosamplesize", audio->getAudioSampleBit());
        }
        if(audio->getAudioChannel() > 0){
            _metedata.set("audiochannels", audio->getAudioChannel());
            _metedata.set("stereo", audio->getAudioChannel() > 1);
        }
        _codecId = audio->getCodecId();
        _metedata.set("audiocodecid", Factory::getAmfByCodecId(_codecId));
    }
    virtual ~AudioMete(){}

    /**
     * 返回音频或视频类型
     * @return
     */
    TrackType getTrackType() const override {
        return TrackAudio;
    }

    /**
     * 返回编码器id
     * @return
     */
    CodecId getCodecId() const override{
        return _codecId;
    }
private:
    CodecId _codecId;
};
































}//namespace mediakit

#endif //ZLMEDIAKIT_RTMPMETEDATA_H
