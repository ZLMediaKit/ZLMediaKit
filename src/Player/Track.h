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

#ifndef ZLMEDIAKIT_TRACK_H
#define ZLMEDIAKIT_TRACK_H

#include <memory>
#include <string>
#include "Frame.h"
#include "Util/RingBuffer.h"
#include "Rtsp/Rtsp.h"
#include "Player.h"
using namespace toolkit;

namespace mediakit{

/**
 * 媒体通道描述类，也支持帧输入输出
 */
class Track : public FrameRingInterfaceDelegate , public CodecInfo{
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
    virtual int getVideoHeight() const = 0;

    /**
     * 返回视频宽度
     * @return
     */
    virtual int getVideoWidth() const  = 0;

    /**
     * 返回视频fps
     * @return
     */
    virtual float getVideoFps() const = 0;
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
    virtual int getAudioSampleRate() const  = 0;

    /**
     * 返回音频采样位数，一般为16或8
     * @return
     */
    virtual int getAudioSampleBit() const = 0;

    /**
     * 返回音频通道数
     * @return
     */
    virtual int getAudioChannel() const = 0;
};

/**
 * 264视频通道
 */
class H264Track : public VideoTrack{
public:
    typedef std::shared_ptr<H264Track> Ptr;

    /**
     * 不指定sps pps构造h264类型的媒体
     * 在随后的inputFrame中获取sps pps
     */
    H264Track(){}
    /**
     * 构造h264类型的媒体
     * @param sps sps帧数据
     * @param pps pps帧数据
     * @param sps_prefix_len 264头长度，可以为3个或4个字节，一般为0x00 00 00 01
     * @param pps_prefix_len 264头长度，可以为3个或4个字节，一般为0x00 00 00 01
     */
    H264Track(const string &sps,const string &pps,int sps_prefix_len = 4,int pps_prefix_len = 4){
        _sps = sps.substr(sps_prefix_len);
        _pps = pps.substr(pps_prefix_len);
        parseSps(_sps);
    }

    /**
     * 构造h264类型的媒体
     * @param sps sps帧
     * @param pps pps帧
     */
    H264Track(const Frame::Ptr &sps,const Frame::Ptr &pps){
        if(sps->getCodecId() != CodecH264 || pps->getCodecId() != CodecH264 ){
            throw std::invalid_argument("必须输入H264类型的帧");
        }
        _sps = string(sps->data() + sps->prefixSize(),sps->size() - sps->prefixSize());
        _pps = string(pps->data() + pps->prefixSize(),pps->size() - pps->prefixSize());
        parseSps(_sps);
    }

    /**
     * 返回不带0x00 00 00 01头的sps
     * @return
     */
    const string &getSps() const{
        return _sps;
    }

    /**
     * 返回不带0x00 00 00 01头的pps
     * @return
     */
    const string &getPps() const{
        return _pps;
    }

    CodecId getCodecId() const override {
        return CodecH264;
    }

    /**
     * 返回视频高度
     * @return
     */
    int getVideoHeight() const override{
        return _width;
    }

    /**
     * 返回视频宽度
     * @return
     */
    int getVideoWidth() const override{
        return _height;
    }

    /**
     * 返回视频fps
     * @return
     */
    float getVideoFps() const override{
        return _fps;
    }

    bool ready() override {
        return !_sps.empty() && !_pps.empty();
    }


    /**
     * 输入数据帧,并获取sps pps
     * @param frame 数据帧
     */
    void inputFrame(const Frame::Ptr &frame) override{
        int type = (*((uint8_t *)frame->data() + frame->prefixSize())) & 0x1F;
        switch (type){
            case 7:{
                //sps
                bool flag = _sps.empty();
                _sps = string(frame->data() + frame->prefixSize(),frame->size() - frame->prefixSize());
                if(flag && _width == 0){
                    parseSps(_sps);
                }
            }
                break;
            case 8:{
                //pps
                _pps = string(frame->data() + frame->prefixSize(),frame->size() - frame->prefixSize());
            }
                break;

            case 5:{
                //I
                if(!_sps.empty()){
                    H264Frame::Ptr insertFrame = std::make_shared<H264Frame>();
                    insertFrame->timeStamp = frame->stamp();
                    insertFrame->type = 7;
                    insertFrame->buffer = _sps;
                    insertFrame->iPrefixSize = 0;
                    VideoTrack::inputFrame(insertFrame);
                }

                if(!_pps.empty()){
                    H264Frame::Ptr insertFrame = std::make_shared<H264Frame>();
                    insertFrame->timeStamp = frame->stamp();
                    insertFrame->type = 8;
                    insertFrame->buffer = _pps;
                    insertFrame->iPrefixSize = 0;
                    VideoTrack::inputFrame(insertFrame);
                }
                VideoTrack::inputFrame(frame);
            }
                break;

            case 1:{
                //B or P
                VideoTrack::inputFrame(frame);
            }
                break;
        }
    }
private:
    /**
     * 解析sps获取宽高fps
     * @param sps sps不含头数据
     */
    void parseSps(const string &sps){
        getAVCInfo(sps,_width,_height,_fps);
    }
    Track::Ptr clone() override {
        return std::make_shared<std::remove_reference<decltype(*this)>::type >(*this);
    }
private:
    string _sps;
    string _pps;
    int _width = 0;
    int _height = 0;
    float _fps = 0;
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
        if(aac_cfg.size() != 2){
            throw std::invalid_argument("adts配置必须为2个字节");
        }
        _cfg = aac_cfg;
        parseAacCfg(_cfg);
    }

    /**
     * 构造aac类型的媒体
     * @param adts_header adts头，7个字节
     * @param adts_header_len adts头长度，不少于7个字节
     */
    AACTrack(const char *adts_header,int adts_header_len = 7){
        if(adts_header_len < 7){
            throw std::invalid_argument("adts头必须不少于7个字节");
        }
        _cfg = makeAdtsConfig((uint8_t*)adts_header);
        parseAacCfg(_cfg);
    }

    /**
     * 构造aac类型的媒体
     * @param aac_frame_with_adts 带adts头的aac帧
     */
    AACTrack(const Frame::Ptr &aac_frame_with_adts){
        if(aac_frame_with_adts->getCodecId() != CodecAAC || aac_frame_with_adts->prefixSize() < 7){
            throw std::invalid_argument("必须输入带adts头的aac帧");
        }
        _cfg = makeAdtsConfig((uint8_t*)aac_frame_with_adts->data());
        parseAacCfg(_cfg);
    }

    /**
     * 获取aac两个字节的配置
     * @return
     */
    const string &getAacCfg() const{
        return _cfg;
    }

    /**
     * 返回编码类型
     * @return
     */
    CodecId getCodecId() const override{
        return CodecAAC;
    }

    /**
     * 在获取aac_cfg前是无效的Track
     * @return
     */
    bool ready() override {
        return !_cfg.empty();
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
        if(_cfg.empty() && frame->prefixSize() >= 7){
            //7个字节的adts头
            _cfg = makeAdtsConfig(reinterpret_cast<const uint8_t *>(frame->data()));
            parseAacCfg(_cfg);
        }
        AudioTrack::inputFrame(frame);
    }
private:
    /**
     * 解析2个字节的aac配置
     * @param aac_cfg
     */
    void parseAacCfg(const string &aac_cfg){
        AACFrame aacFrame;
        makeAdtsHeader(aac_cfg,aacFrame);
        getAACInfo(aacFrame,_sampleRate,_channel);
    }
    Track::Ptr clone() override {
        return std::make_shared<std::remove_reference<decltype(*this)>::type >(*this);
    }
private:
    string _cfg;
    int _sampleRate = 0;
    int _sampleBit = 16;
    int _channel = 0;
};

}//namespace mediakit

#endif //ZLMEDIAKIT_TRACK_H
