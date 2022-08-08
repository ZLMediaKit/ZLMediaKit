/*
 * Copyright (c) 2016 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/xia-chu/ZLMediaKit).
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
#include "Util/util.h"
#include "Common/config.h"
#include "Common/macros.h"
#include "Extension/Frame.h"

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

#if defined(_WIN32)
#pragma pack(push, 1)
#endif // defined(_WIN32)

class RtpHeader {
public:
#if __BYTE_ORDER == __BIG_ENDIAN
    //版本号，固定为2
    uint32_t version: 2;
    //padding
    uint32_t padding: 1;
    //扩展
    uint32_t ext: 1;
    //csrc
    uint32_t csrc: 4;
    //mark
    uint32_t mark: 1;
    //负载类型
    uint32_t pt: 7;
#else
    //csrc
    uint32_t csrc: 4;
    //扩展
    uint32_t ext: 1;
    //padding
    uint32_t padding: 1;
    //版本号，固定为2
    uint32_t version: 2;
    //负载类型
    uint32_t pt: 7;
    //mark
    uint32_t mark: 1;
#endif
    //序列号
    uint32_t seq: 16;
    //时间戳
    uint32_t stamp;
    //ssrc
    uint32_t ssrc;
    //负载，如果有csrc和ext，前面为 4 * csrc + (4 + 4 * ext_len)
    uint8_t payload;

public:
    //返回csrc字段字节长度
    size_t getCsrcSize() const;
    //返回csrc字段首地址，不存在时返回nullptr
    uint8_t *getCsrcData();

    //返回ext字段字节长度
    size_t getExtSize() const;
    //返回ext reserved值
    uint16_t getExtReserved() const;
    //返回ext段首地址，不存在时返回nullptr
    uint8_t *getExtData();

    //返回有效负载指针,跳过csrc、ext
    uint8_t* getPayloadData();
    //返回有效负载总长度,不包括csrc、ext、padding
    ssize_t getPayloadSize(size_t rtp_size) const;
    //打印调试信息
    std::string dumpString(size_t rtp_size) const;

private:
    //返回有效负载偏移量
    size_t getPayloadOffset() const;
    //返回padding长度
    size_t getPaddingSize(size_t rtp_size) const;
} PACKED;

#if defined(_WIN32)
#pragma pack(pop)
#endif // defined(_WIN32)

//此rtp为rtp over tcp形式，需要忽略前4个字节
class RtpPacket : public toolkit::BufferRaw{
public:
    using Ptr = std::shared_ptr<RtpPacket>;
    enum {
        kRtpVersion = 2,
        kRtpHeaderSize = 12,
        kRtpTcpHeaderSize = 4
    };

    //获取rtp头
    RtpHeader* getHeader();
    const RtpHeader* getHeader() const;

    //打印调试信息
    std::string dumpString() const;

    //主机字节序的seq
    uint16_t getSeq() const;
    uint32_t getStamp() const;
    //主机字节序的时间戳，已经转换为毫秒
    uint64_t getStampMS(bool ntp = true) const;
    //主机字节序的ssrc
    uint32_t getSSRC() const;
    //有效负载，跳过csrc、ext
    uint8_t* getPayload();
    //有效负载长度，不包括csrc、ext、padding
    size_t getPayloadSize() const;

    //音视频类型
    TrackType type;
    //音频为采样率，视频一般为90000
    uint32_t sample_rate;
    //ntp时间戳
    uint64_t ntp_stamp;

    static Ptr create();

private:
    friend class toolkit::ResourcePool_l<RtpPacket>;
    RtpPacket() = default;

private:
    //对象个数统计
    toolkit::ObjectStatistic<RtpPacket> _statistic;
};

class RtpPayload {
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

class SdpTrack {
public:
    using Ptr = std::shared_ptr<SdpTrack>;
    std::string _t;
    std::string _b;
    uint16_t _port;

    float _duration = 0;
    float _start = 0;
    float _end = 0;

    std::map<char, std::string> _other;
    std::multimap<std::string, std::string> _attr;

    std::string toString(uint16_t port = 0) const;
    std::string getName() const;
    std::string getControlUrl(const std::string &base_url) const;

public:
    int _pt = 0xff;
    int _channel;
    int _samplerate;
    TrackType _type;
    std::string _codec;
    std::string _fmtp;
    std::string _control;

public:
    bool _inited = false;
    uint8_t _interleaved = 0;
    uint16_t _seq = 0;
    uint32_t _ssrc = 0;
    //时间戳，单位毫秒
    uint32_t _time_stamp = 0;
};

class SdpParser {
public:
    using Ptr = std::shared_ptr<SdpParser>;

    SdpParser() {}
    SdpParser(const std::string &sdp) { load(sdp); }
    ~SdpParser() {}

    void load(const std::string &sdp);
    bool available() const;
    SdpTrack::Ptr getTrack(TrackType type) const;
    std::vector<SdpTrack::Ptr> getAvailableTrack() const;
    std::string toString() const;

private:
    std::vector<SdpTrack::Ptr> _track_vec;
};

/**
* rtsp sdp基类
*/
class Sdp : public CodecInfo{
public:
    using Ptr = std::shared_ptr<Sdp>;

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
    virtual std::string getSdp() const  = 0;

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
    using Ptr = std::shared_ptr<TitleSdp>;
    /**
     * 构造title类型sdp
     * @param dur_sec rtsp点播时长，0代表直播，单位秒
     * @param header 自定义sdp描述
     * @param version sdp版本
     */
    TitleSdp(float dur_sec = 0,
             const std::map<std::string, std::string> &header = std::map<std::string, std::string>(),
             int version = 0) : Sdp(0, 0) {
        _printer << "v=" << version << "\r\n";

        if (!header.empty()) {
            for (auto &pr : header) {
                _printer << pr.first << "=" << pr.second << "\r\n";
            }
        } else {
            _printer << "o=- 0 0 IN IP4 0.0.0.0\r\n";
            _printer << "s=Streamed by " << kServerName << "\r\n";
            _printer << "c=IN IP4 0.0.0.0\r\n";
            _printer << "t=0 0\r\n";
        }

        if (dur_sec <= 0) {
            //直播
            _printer << "a=range:npt=now-\r\n";
        } else {
            //点播
            _dur_sec = dur_sec;
            _printer << "a=range:npt=0-" << dur_sec << "\r\n";
        }
        _printer << "a=control:*\r\n";
    }

    std::string getSdp() const override {
        return _printer;
    }

    CodecId getCodecId() const override {
        return CodecInvalid;
    }

    float getDuration() const {
        return _dur_sec;
    }

private:
    float _dur_sec = 0;
    toolkit::_StrPrinter _printer;
};

//创建rtp over tcp4个字节的头
toolkit::Buffer::Ptr makeRtpOverTcpPrefix(uint16_t size, uint8_t interleaved);
//创建rtp-rtcp端口对
void makeSockPair(std::pair<toolkit::Socket::Ptr, toolkit::Socket::Ptr> &pair, const std::string &local_ip, bool re_use_port = false, bool is_udp = true);
//十六进制方式打印ssrc
std::string printSSRC(uint32_t ui32Ssrc);

} //namespace mediakit
#endif //RTSP_RTSP_H_
