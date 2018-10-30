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
#ifndef ZLMEDIAKIT_RTSPSDP_H
#define ZLMEDIAKIT_RTSPSDP_H

#include "Util/base64.h"
#include "Extension/Track.h"
#include "RtspMuxer/RtpCodec.h"

namespace mediakit {

/**
* rtsp sdp基类
*/
class Sdp : public CodecInfo{
public:
    typedef std::shared_ptr<Sdp> Ptr;

    /**
     * 构造sdp
     * @param sample_rate 采样率
     * @param playload_type pt类型
     */
    Sdp(uint32_t sample_rate, uint8_t playload_type){
        _sample_rate = sample_rate;
        _playload_type = playload_type;
    }

    virtual ~Sdp(){}

    /**
     * 获取sdp字符串
     * @return
     */
    virtual string getSdp() const  = 0;


    /**
     * 创建Rtp打包器
     * @param ssrc 打包器ssrc，可以为0
     * @param mtu mtu大小，一般小于1500字节，推荐1400
     * @return Rtp打包器
     */
    virtual RtpCodec::Ptr  createRtpEncoder(uint32_t ssrc, int mtu);
private:
    uint8_t _playload_type;
    uint32_t _sample_rate;
};

/**
* sdp中除音视频外的其他描述部分
*/
class TitleSdp : public Sdp{
public:

    /**
     * 构造title类型sdp
     * @param dur_sec rtsp点播时长，0代表直播，单位秒
     * @param header 自定义sdp描述
     * @param version sdp版本
     */
    TitleSdp(float dur_sec = 0,
             const map<string,string> &header = map<string,string>(),
             int version = 0) : Sdp(0,0){
        _printer << "v=" << version << "\r\n";

        if(!header.empty()){
            for (auto &pr : header){
                _printer << pr.first << "=" << pr.second << "\r\n";
            }
        } else {
            _printer << "o=- 1383190487994921 1 IN IP4 0.0.0.0\r\n";
            _printer << "s=RTSP Session, streamed by the ZLMediaKit\r\n";
            _printer << "i=ZLMediaKit Live Stream\r\n";
            _printer << "c=IN IP4 0.0.0.0\r\n";
            _printer << "t=0 0\r\n";
        }

        if(dur_sec <= 0){
            _printer << "a=range:npt=0-\r\n";
        }else{
            _printer << "a=range:npt=0-" << dur_sec  << "\r\n";
        }
        _printer << "a=control:*\r\n";
    }
    string getSdp() const override {
        return _printer;
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
private:
    _StrPrinter _printer;
};


} /* namespace mediakit */



#endif //ZLMEDIAKIT_RTSPSDP_H
