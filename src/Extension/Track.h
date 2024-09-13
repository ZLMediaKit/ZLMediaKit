/*
 * Copyright (c) 2016-present The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/ZLMediaKit/ZLMediaKit).
 *
 * Use of this source code is governed by MIT-like license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#ifndef ZLMEDIAKIT_TRACK_H
#define ZLMEDIAKIT_TRACK_H

#include <memory>
#include <string>
#include "Frame.h"
#include "Rtsp/Rtsp.h"

namespace mediakit{

/**
 * 媒体通道描述类，也支持帧输入输出
 * Media channel description class, also supports frame input and output
 
 * [AUTO-TRANSLATED:a3acd089]
 */
class Track : public FrameDispatcher, public CodecInfo {
public:
    using Ptr = std::shared_ptr<Track>;

    /**
     * 默认构造
     * Default constructor
     
     * [AUTO-TRANSLATED:acda54ab]
     */
    Track() = default;

    /**
     * 复制拷贝，只能拷贝派生类的信息，
     * 环形缓存和代理关系不能拷贝，否则会关系紊乱
     * Copy, only copy information of derived classes,
     * Circular buffer and proxy relationships cannot be copied, otherwise the relationship will be disordered
     
     * [AUTO-TRANSLATED:308e6502]
     */
    Track(const Track &that) {
        _bit_rate = that._bit_rate;
        setIndex(that.getIndex());
    }

    /**
     * 是否准备好，准备好才能获取譬如sps pps等信息
     * Whether it is ready, it can be used to get information such as sps pps
     
     * [AUTO-TRANSLATED:6d819ef7]
     */
    virtual bool ready() const = 0;

    /**
     * 克隆接口，用于复制本对象用
     * 在调用该接口时只会复制派生类的信息
     * 环形缓存和代理关系不能拷贝，否则会关系紊乱
     * Clone interface, used to copy this object
     * When calling this interface, only the information of the derived class will be copied
     * Circular buffer and proxy relationships cannot be copied, otherwise the relationship will be disordered
     
     * [AUTO-TRANSLATED:270874c6]
     */
    virtual Track::Ptr clone() const = 0;

    /**
     * 更新track信息，比如触发sps/pps解析
     * Update track information, such as triggering sps/pps parsing
     
     * [AUTO-TRANSLATED:324879ef]
     */
    virtual bool update() { return false; }

    /**
     * 生成sdp
     * @return sdp对象
     * Generate sdp
     * @return sdp object
     
     * [AUTO-TRANSLATED:3ab2fd30]
     */
    virtual Sdp::Ptr getSdp(uint8_t payload_type) const = 0;

    /**
     * 获取extra data, 一般用于rtmp/mp4生成
     * Get extra data, generally used for rtmp/mp4 generation
     
     * [AUTO-TRANSLATED:d8ff2cd5]
     */
    virtual toolkit::Buffer::Ptr getExtraData() const { return nullptr; }

    /**
     * 设置extra data，
     * Set extra data,
     
     * [AUTO-TRANSLATED:9e551857]
     */
    virtual void setExtraData(const uint8_t *data, size_t size) {}

    /**
     * 返回比特率
     * @return 比特率
     * Return bitrate
     * @return Bitrate
     
     * [AUTO-TRANSLATED:265dda35]
     */
    virtual int getBitRate() const { return _bit_rate; }

    /**
     * 设置比特率
     * @param bit_rate 比特率
     * Set bitrate
     * @param bit_rate Bitrate
     
     * [AUTO-TRANSLATED:77a43064]
     */
    virtual void setBitRate(int bit_rate) { _bit_rate = bit_rate; }

private:
    int _bit_rate = 0;
};

/**
 * 视频通道描述Track类，支持获取宽高fps信息
 * Video channel description Track class, supports getting width, height and fps information
 
 * [AUTO-TRANSLATED:8d1893c5]
 */
class VideoTrack : public Track {
public:
    using Ptr = std::shared_ptr<VideoTrack>;

    /**
     * 返回视频高度
     * Return video height
     
     * [AUTO-TRANSLATED:b24aabc0]
     */
    virtual int getVideoHeight() const { return 0; }

    /**
     * 返回视频宽度
     * Return video width
     
     * [AUTO-TRANSLATED:2f3bb6e3]
     */
    virtual int getVideoWidth() const { return 0; }

    /**
     * 返回视频fps
     * Return video fps
     
     * [AUTO-TRANSLATED:ced99aef]
     */
    virtual float getVideoFps() const { return 0; }

    /**
     * 返回相关 sps/pps 等
     * Return related sps/pps, etc.
     
     * [AUTO-TRANSLATED:30fc4f63]
     */
    virtual std::vector<Frame::Ptr> getConfigFrames() const { return std::vector<Frame::Ptr>{}; }
};

class VideoTrackImp : public VideoTrack {
public:
    using Ptr = std::shared_ptr<VideoTrackImp>;

    /**
     * 构造函数
     * @param codec_id 编码类型
     * @param width 宽
     * @param height 高
     * @param fps 帧率
     * Constructor
     * @param codec_id Encoding type
     * @param width Width
     * @param height Height
     * @param fps Frame rate
     
     * [AUTO-TRANSLATED:b3d1ef4d]
     */
    VideoTrackImp(CodecId codec_id, int width, int height, int fps) {
        _codec_id = codec_id;
        _width = width;
        _height = height;
        _fps = fps;
    }

    int getVideoWidth() const override { return _width; }
    int getVideoHeight() const override { return _height; }
    float getVideoFps() const override { return _fps; }
    bool ready() const override { return true; }

    Track::Ptr clone() const override { return std::make_shared<VideoTrackImp>(*this); }
    Sdp::Ptr getSdp(uint8_t payload_type) const override { return nullptr; }
    CodecId getCodecId() const override { return _codec_id; }

private:
    CodecId _codec_id;
    int _width = 0;
    int _height = 0;
    float _fps = 0;
};

/**
 * 音频Track派生类，支持采样率通道数，采用位数信息
 * Audio Track derived class, supports sampling rate, number of channels, and sampling bit information
 
 * [AUTO-TRANSLATED:5f57819d]
 */
class AudioTrack : public Track {
public:
    using Ptr = std::shared_ptr<AudioTrack>;

    /**
     * 返回音频采样率
     * Return audio sampling rate
     
     * [AUTO-TRANSLATED:9af5a0a4]
     */
    virtual int getAudioSampleRate() const  {return 0;};

    /**
     * 返回音频采样位数，一般为16或8
     * Return audio sampling bit depth, generally 16 or 8
     
     * [AUTO-TRANSLATED:5fedc65d]
     */
    virtual int getAudioSampleBit() const {return 0;};

    /**
     * 返回音频通道数
     * Return audio number of channels
     
     * [AUTO-TRANSLATED:2613b317]
     */
    virtual int getAudioChannel() const {return 0;};
};

class AudioTrackImp : public AudioTrack{
public:
    using Ptr = std::shared_ptr<AudioTrackImp>;

    /**
     * 构造函数
     * @param codecId 编码类型
     * @param sample_rate 采样率(HZ)
     * @param channels 通道数
     * @param sample_bit 采样位数，一般为16
     * Constructor
     * @param codecId Encoding type
     * @param sample_rate Sampling rate (HZ)
     * @param channels Number of channels
     * @param sample_bit Sampling bit depth, generally 16
     
     * [AUTO-TRANSLATED:0ad0211f]
     */
    AudioTrackImp(CodecId codecId, int sample_rate, int channels, int sample_bit){
        _codecid = codecId;
        _sample_rate = sample_rate;
        _channels = channels;
        _sample_bit = sample_bit;
    }

    /**
     * 返回编码类型
     * Return encoding type
     
     * [AUTO-TRANSLATED:c8731864]
     */
    CodecId getCodecId() const override{
        return _codecid;
    }

    /**
     * 是否已经初始化
     * Whether it has been initialized
     
     * [AUTO-TRANSLATED:5dc6693e]
     */
    bool ready() const override {
        return true;
    }

    /**
     * 返回音频采样率
     * Return audio sampling rate
     
     * [AUTO-TRANSLATED:9af5a0a4]
     */
    int getAudioSampleRate() const override{
        return _sample_rate;
    }

    /**
     * 返回音频采样位数，一般为16或8
     * Return audio sampling bit depth, generally 16 or 8
     
     * [AUTO-TRANSLATED:5fedc65d]
     */
    int getAudioSampleBit() const override{
        return _sample_bit;
    }

    /**
     * 返回音频通道数
     * Return audio number of channels
     
     * [AUTO-TRANSLATED:2613b317]
     */
    int getAudioChannel() const override{
        return _channels;
    }

    Track::Ptr clone() const override { return std::make_shared<AudioTrackImp>(*this); }
    Sdp::Ptr getSdp(uint8_t payload_type) const override { return nullptr; }

private:
    CodecId _codecid;
    int _sample_rate;
    int _channels;
    int _sample_bit;
};

class TrackSource {
public:
    virtual ~TrackSource() = default;

    /**
     * 获取全部的Track
     * @param trackReady 是否获取全部已经准备好的Track
     * Get all Tracks
     * @param trackReady Whether to get all ready Tracks
     
     * [AUTO-TRANSLATED:f0779985]
     */
    virtual std::vector<Track::Ptr> getTracks(bool trackReady = true) const = 0;

    /**
     * 获取特定Track
     * @param type track类型
     * @param trackReady 是否获取全部已经准备好的Track
     * Get specific Track
     * @param type Track type
     * @param trackReady Whether to get all ready Tracks
     * [AUTO-TRANSLATED:c50781b9]
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