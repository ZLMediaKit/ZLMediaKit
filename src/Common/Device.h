/*
 * Copyright (c) 2016 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/xia-chu/ZLMediaKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#ifndef DEVICE_DEVICE_H_
#define DEVICE_DEVICE_H_

#include <memory>
#include <string>
#include <functional>
#include "Util/util.h"
#include "Util/TimeTicker.h"
#include "Common/MultiMediaSourceMuxer.h"
using namespace std;
using namespace toolkit;

namespace mediakit {

class H264Encoder;
class AACEncoder;

class VideoInfo {
public:
    CodecId codecId = CodecH264;
    int iWidth;
    int iHeight;
    float iFrameRate;
};

class AudioInfo {
public:
    CodecId codecId = CodecAAC;
    int iChannel;
    int iSampleBit;
    int iSampleRate;
};

/**
 * MultiMediaSourceMuxer类的包装，方便初学者使用
 */
class DevChannel  : public MultiMediaSourceMuxer{
public:
    typedef std::shared_ptr<DevChannel> Ptr;
    //fDuration<=0为直播，否则为点播
    DevChannel(const string &vhost, const string &app, const string &stream_id,
               float duration = 0, bool enable_hls = true, bool enable_mp4 = false);

    ~DevChannel() override ;

    /**
     * 初始化视频Track
     * 相当于MultiMediaSourceMuxer::addTrack(VideoTrack::Ptr );
     * @param info 视频相关信息
     */
    void initVideo(const VideoInfo &info);

    /**
     * 初始化音频Track
     * 相当于MultiMediaSourceMuxer::addTrack(AudioTrack::Ptr );
     * @param info 音频相关信息
     */
    void initAudio(const AudioInfo &info);

    /**
     * 输入264帧
     * @param data 264单帧数据指针
     * @param len 数据指针长度
     * @param dts 解码时间戳，单位毫秒；等于0时内部会自动生成时间戳
     * @param pts 播放时间戳，单位毫秒；等于0时内部会赋值为dts
     */
    void inputH264(const char *data, int len, uint32_t dts, uint32_t pts = 0);

    /**
     * 输入265帧
     * @param data 265单帧数据指针
     * @param len 数据指针长度
     * @param dts 解码时间戳，单位毫秒；等于0时内部会自动生成时间戳
     * @param pts 播放时间戳，单位毫秒；等于0时内部会赋值为dts
     */
    void inputH265(const char *data, int len, uint32_t dts, uint32_t pts = 0);

    /**
     * 输入aac帧
     * @param data_without_adts 不带adts头的aac帧
     * @param len 帧数据长度
     * @param dts 时间戳，单位毫秒
     * @param adts_header adts头
     */
    void inputAAC(const char *data_without_adts, int len, uint32_t dts, const char *adts_header);

    /**
     * 输入OPUS/G711音频帧
     * @param data 音频帧
     * @param len 帧数据长度
     * @param dts 时间戳，单位毫秒
     */
    void inputAudio(const char *data, int len, uint32_t dts);

    /**
     * 输入yuv420p视频帧，内部会完成编码并调用inputH264方法
     * @param apcYuv
     * @param aiYuvLen
     * @param uiStamp
     */
    void inputYUV(char *apcYuv[3], int aiYuvLen[3], uint32_t uiStamp);


    /**
     * 输入pcm数据，内部会完成编码并调用inputAAC方法
     * @param pcData
     * @param iDataLen
     * @param uiStamp
     */
    void inputPCM(char *pcData, int iDataLen, uint32_t uiStamp);

private:
    MediaOriginType getOriginType(MediaSource &sender) const override;

private:
    std::shared_ptr<H264Encoder> _pH264Enc;
    std::shared_ptr<AACEncoder> _pAacEnc;
    std::shared_ptr<VideoInfo> _video;
    std::shared_ptr<AudioInfo> _audio;
    SmoothTicker _aTicker[2];
};

} /* namespace mediakit */

#endif /* DEVICE_DEVICE_H_ */
