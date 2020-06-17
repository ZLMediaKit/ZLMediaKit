/*
 * Copyright (c) 2016 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/xiongziliang/ZLMediaKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#ifndef RTSP_RTSP_H_
#define RTSP_RTSP_H_

#include <string.h>
#include <string>
#include <memory>
#include <unordered_map>
#include "Common/config.h"
#include "Util/util.h"
#include "Extension/Frame.h"

using namespace std;
using namespace toolkit;
using namespace mediakit;

namespace mediakit {

namespace Rtsp {
typedef enum {
    RTP_Invalid = -1,
    RTP_TCP = 0,
    RTP_UDP = 1,
    RTP_MULTICAST = 2,
} eRtpType;

#define RTP_PT_MAP(XX) \
    XX(PCMU, TrackAudio, 0, 8000, 1, CodecG711U) \
    XX(GSM, TrackAudio , 3, 8000, 1, CodecInvalid) \
    XX(G723, TrackAudio, 4, 8000, 1, CodecInvalid) \
    XX(DVI4_8000, TrackAudio, 5, 8000, 1, CodecInvalid) \
    XX(DVI4_16000, TrackAudio, 6, 16000, 1, CodecInvalid) \
    XX(LPC, TrackAudio, 7, 8000, 1, CodecInvalid) \
    XX(PCMA, TrackAudio, 8, 8000, 1, CodecG711A) \
    XX(G722, TrackAudio, 9, 8000, 1, CodecInvalid) \
    XX(L16_Stereo, TrackAudio, 10, 44100, 2, CodecInvalid) \
    XX(L16_Mono, TrackAudio, 11, 44100, 1, CodecInvalid) \
    XX(QCELP, TrackAudio, 12, 8000, 1, CodecInvalid) \
    XX(CN, TrackAudio, 13, 8000, 1, CodecInvalid) \
    XX(MPA, TrackAudio, 14, 90000, 1, CodecInvalid) \
    XX(G728, TrackAudio, 15, 8000, 1, CodecInvalid) \
    XX(DVI4_11025, TrackAudio, 16, 11025, 1, CodecInvalid) \
    XX(DVI4_22050, TrackAudio, 17, 22050, 1, CodecInvalid) \
    XX(G729, TrackAudio, 18, 8000, 1, CodecInvalid) \
    XX(CelB, TrackVideo, 25, 90000, 1, CodecInvalid) \
    XX(JPEG, TrackVideo, 26, 90000, 1, CodecInvalid) \
    XX(nv, TrackVideo, 28, 90000, 1, CodecInvalid) \
    XX(H261, TrackVideo, 31, 90000, 1, CodecInvalid) \
    XX(MPV, TrackVideo, 32, 90000, 1, CodecInvalid) \
    XX(MP2T, TrackVideo, 33, 90000, 1, CodecInvalid) \
    XX(H263, TrackVideo, 34, 90000, 1, CodecInvalid) \

typedef enum {
#define ENUM_DEF(name, type, value, clock_rate, channel, codec_id) PT_ ## name = value,
    RTP_PT_MAP(ENUM_DEF)
#undef ENUM_DEF
    PT_MAX = 128
} PayloadType;

};

class RtpPacket : public BufferRaw{
public:
    typedef std::shared_ptr<RtpPacket> Ptr;
    uint8_t interleaved;
    uint8_t PT;
    bool mark;
    //时间戳，单位毫秒
    uint32_t timeStamp;
    uint16_t sequence;
    uint32_t ssrc;
    uint32_t offset;
    TrackType type;
};

class RtpPayload{
public:
    static int getClockRate(int pt);
    static TrackType getTrackType(int pt);
    static int getAudioChannel(int pt);
    static const char *getName(int pt);
    static CodecId getCodecId(int pt);
private:
    RtpPayload() = delete;
    ~RtpPayload() = delete;
};

class RtcpCounter {
public:
    uint32_t pktCnt = 0;
    uint32_t octCount = 0;
    //网络字节序
    uint32_t timeStamp = 0;
    uint32_t lastTimeStamp = 0;
};

class SdpTrack {
public:
    typedef std::shared_ptr<SdpTrack> Ptr;

    string _m;
    string _o;
    string _s;
    string _i;
    string _c;
    string _t;
    string _b;
    uint16_t _port;

    float _duration = 0;
    float _start = 0;
    float _end = 0;

    map<char, string> _other;
    map<string, string> _attr;

    string toString() const;
    string getName() const;
public:
    int _pt;
    string _codec;
    int _samplerate;
    int _channel;
    string _fmtp;
    string _control;
    string _control_surffix;
    TrackType _type;
public:
    uint8_t _interleaved = 0;
    bool _inited = false;
    uint32_t _ssrc = 0;
    uint16_t _seq = 0;
    //时间戳，单位毫秒
    uint32_t _time_stamp = 0;
};

class SdpParser {
public:
    typedef std::shared_ptr<SdpParser> Ptr;

    SdpParser() {}
    SdpParser(const string &sdp) { load(sdp); }
    ~SdpParser() {}
    void load(const string &sdp);
    bool available() const;
    SdpTrack::Ptr getTrack(TrackType type) const;
    vector<SdpTrack::Ptr> getAvailableTrack() const;
    string toString() const ;
private:
    vector<SdpTrack::Ptr> _track_vec;
};

/**
 * 解析rtsp url的工具类
 */
class RtspUrl{
public:
    string _url;
    string _user;
    string _passwd;
    string _host;
    uint16_t _port;
    bool _is_ssl;
public:
    RtspUrl() = default;
    ~RtspUrl() = default;
    bool parse(const string &url);
private:
    bool setup(bool,const string &, const string &, const string &);
};

/**
* rtsp sdp基类
*/
class Sdp : public CodecInfo{
public:
    typedef std::shared_ptr<Sdp> Ptr;

    /**
     * 构造sdp
     * @param sample_rate 采样率
     * @param payload_type pt类型
     */
    Sdp(uint32_t sample_rate, uint8_t payload_type){
        _sample_rate = sample_rate;
        _payload_type = payload_type;
    }

    virtual ~Sdp(){}

    /**
     * 获取sdp字符串
     * @return
     */
    virtual string getSdp() const  = 0;

    /**
     * 获取pt
     * @return
     */
    uint8_t getPayloadType() const{
        return _payload_type;
    }

    /**
     * 获取采样率
     * @return
     */
    uint32_t getSampleRate() const{
        return _sample_rate;
    }
private:
    uint8_t _payload_type;
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
            _printer << "o=- 0 0 IN IP4 0.0.0.0\r\n";
            _printer << "s=Streamed by " << SERVER_NAME << "\r\n";
            _printer << "c=IN IP4 0.0.0.0\r\n";
            _printer << "t=0 0\r\n";
        }

        if(dur_sec <= 0){
            //直播
            _printer << "a=range:npt=now-\r\n";
        }else{
            //点播
            _printer << "a=range:npt=0-" << dur_sec  << "\r\n";
        }
        _printer << "a=control:*\r\n";
    }
    string getSdp() const override {
        return _printer;
    }

    CodecId getCodecId() const override{
        return CodecInvalid;
    }
private:
    _StrPrinter _printer;
};

std::pair<Socket::Ptr, Socket::Ptr> makeSockPair(const EventPoller::Ptr &poller, const string &local_ip);

} //namespace mediakit
#endif //RTSP_RTSP_H_
