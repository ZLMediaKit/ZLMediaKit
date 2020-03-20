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


#ifdef ENABLE_FAAC
#include "Codec/AACEncoder.h"
#endif //ENABLE_FAAC

#ifdef ENABLE_X264
#include "Codec/H264Encoder.h"
#endif //ENABLE_X264


namespace mediakit {

class VideoInfo {
public:
    int iWidth;
    int iHeight;
    float iFrameRate;
};
class AudioInfo {
public:
    int iChannel;
    int iSampleBit;
    int iSampleRate;
    int iProfile;
};

/**
 * 该类已经废弃，保留只为兼容旧代码，请直接使用MultiMediaSourceMuxer类！
 */
class DevChannel  : public MultiMediaSourceMuxer{
public:
    typedef std::shared_ptr<DevChannel> Ptr;
    //fDuration<=0为直播，否则为点播
    DevChannel(const string &strVhost,
               const string &strApp,
               const string &strId,
               float fDuration = 0,
               bool bEanbleRtsp = true,
               bool bEanbleRtmp = true,
               bool bEanbleHls = true,
               bool bEnableMp4 = false);

    virtual ~DevChannel();

    /**
     * 初始化h264视频Track
     * 相当于MultiMediaSourceMuxer::addTrack(H264Track::Ptr );
     * @param info
     */
    void initVideo(const VideoInfo &info);

    /**
     * 初始化h265视频Track
     * @param info
     */
    void initH265Video(const VideoInfo &info);

    /**
     * 初始化aac音频Track
     * 相当于MultiMediaSourceMuxer::addTrack(AACTrack::Ptr );
     * @param info
     */
    void initAudio(const AudioInfo &info);

    /**
     * 输入264帧
     * @param pcData 264单帧数据指针
     * @param iDataLen 数据指针长度
     * @param dts 解码时间戳，单位毫秒；等于0时内部会自动生成时间戳
     * @param pts 播放时间戳，单位毫秒；等于0时内部会赋值为dts
     */
    void inputH264(const char *pcData, int iDataLen, uint32_t dts,uint32_t pts = 0);

    /**
     * 输入265帧
     * @param pcData 265单帧数据指针
     * @param iDataLen 数据指针长度
     * @param dts 解码时间戳，单位毫秒；等于0时内部会自动生成时间戳
     * @param pts 播放时间戳，单位毫秒；等于0时内部会赋值为dts
     */
    void inputH265(const char *pcData, int iDataLen, uint32_t dts,uint32_t pts = 0);

    /**
     * 输入可能带adts头的aac帧
     * @param pcDataWithAdts 可能带adts头的aac帧
     * @param iDataLen 帧数据长度
     * @param uiStamp 时间戳，单位毫秒，等于0时内部会自动生成时间戳
     * @param withAdtsHeader 是否带adts头
     */
    void inputAAC(const char *pcDataWithAdts, int iDataLen, uint32_t uiStamp, bool withAdtsHeader = true);

    /**
     * 输入不带adts头的aac帧
     * @param pcDataWithoutAdts 不带adts头的aac帧
     * @param iDataLen 帧数据长度
     * @param uiStamp 时间戳，单位毫秒
     * @param pcAdtsHeader adts头
     */
    void inputAAC(const char *pcDataWithoutAdts,int iDataLen, uint32_t uiStamp,const char *pcAdtsHeader);

#ifdef ENABLE_X264
    /**
     * 输入yuv420p视频帧，内部会完成编码并调用inputH264方法
     * @param apcYuv
     * @param aiYuvLen
     * @param uiStamp
     */
    void inputYUV(char *apcYuv[3], int aiYuvLen[3], uint32_t uiStamp);
#endif //ENABLE_X264

#ifdef ENABLE_FAAC

    /**
     * 输入pcm数据，内部会完成编码并调用inputAAC方法
     * @param pcData
     * @param iDataLen
     * @param uiStamp
     */
    void inputPCM(char *pcData, int iDataLen, uint32_t uiStamp);
#endif //ENABLE_FAAC

private:
#ifdef ENABLE_X264
    std::shared_ptr<H264Encoder> _pH264Enc;
#endif //ENABLE_X264

#ifdef ENABLE_FAAC
    std::shared_ptr<AACEncoder> _pAacEnc;
#endif //ENABLE_FAAC
    std::shared_ptr<VideoInfo> _video;
    std::shared_ptr<AudioInfo> _audio;

    SmoothTicker _aTicker[2];
    uint8_t _adtsHeader[7];
};

} /* namespace mediakit */

#endif /* DEVICE_DEVICE_H_ */
