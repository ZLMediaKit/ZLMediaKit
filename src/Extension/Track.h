/*
 * MIT License
 *
 * Copyright (c) 2016-2019 xiongziliang <771730766@qq.com>
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

#ifndef ZLMEDIAKIT_TRACK_H
#define ZLMEDIAKIT_TRACK_H

#include <memory>
#include <string>
#include "Frame.h"
#include "Util/RingBuffer.h"
#include "Rtsp/Rtsp.h"
using namespace toolkit;

namespace mediakit{

/**
 * 媒体通道描述类，也支持帧输入输出
 */
class Track : public FrameDispatcher , public CodecInfo{
public:
    typedef std::shared_ptr<Track> Ptr;
    Track(){}

    virtual ~Track(){}

    /**
     * 是否准备好，准备好才能获取譬如sps pps等信息
     * @return
     */
    virtual bool ready() = 0;

    /**
     * 克隆接口，用于复制本对象用
     * 在调用该接口时只会复制派生类的信息
     * 环形缓存和代理关系不能拷贝，否则会关系紊乱
     * @return
     */
    virtual Track::Ptr clone() = 0;

    /**
     * 生成sdp
     * @return  sdp对象
     */
    virtual Sdp::Ptr getSdp() = 0;

    /**
     * 复制拷贝，只能拷贝派生类的信息，
     * 环形缓存和代理关系不能拷贝，否则会关系紊乱
     * @param that
     */
    Track(const Track &that){}
};

/**
 * 视频通道描述Track类，支持获取宽高fps信息
 */
class VideoTrack : public Track {
public:
    typedef std::shared_ptr<VideoTrack> Ptr;

    TrackType getTrackType() const override { return TrackVideo;};

    /**
     * 返回视频高度
     * @return
     */
    virtual int getVideoHeight() const {return 0;};

    /**
     * 返回视频宽度
     * @return
     */
    virtual int getVideoWidth() const {return 0;};

    /**
     * 返回视频fps
     * @return
     */
    virtual float getVideoFps() const {return 0;};
};

/**
 * 音频Track派生类，支持采样率通道数，采用位数信息
 */
class AudioTrack : public Track {
public:
    typedef std::shared_ptr<AudioTrack> Ptr;

    TrackType getTrackType() const override { return TrackAudio;};

    /**
     * 返回音频采样率
     * @return
     */
    virtual int getAudioSampleRate() const  {return 0;};

    /**
     * 返回音频采样位数，一般为16或8
     * @return
     */
    virtual int getAudioSampleBit() const {return 0;};

    /**
     * 返回音频通道数
     * @return
     */
    virtual int getAudioChannel() const {return 0;};
};


class TrackSource{
public:
    TrackSource(){}
    virtual ~TrackSource(){}

    /**
     * 获取全部的Track
     * @param trackReady 是否获取全部已经准备好的Track
     * @return
     */
    virtual vector<Track::Ptr> getTracks(bool trackReady = true) const = 0;

    /**
     * 获取特定Track
     * @param type track类型
     * @param trackReady 是否获取全部已经准备好的Track
     * @return
     */
    Track::Ptr getTrack(TrackType type , bool trackReady = true) const {
        auto tracks = getTracks(trackReady);
        for(auto &track : tracks){
            if(track->getTrackType() == type){
                return track;
            }
        }
        return nullptr;
    }
};

}//namespace mediakit

#endif //ZLMEDIAKIT_TRACK_H
