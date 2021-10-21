/*
 * Copyright (c) 2016 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/xia-chu/ZLMediaKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#include <stdlib.h>
#include "Rtsp.h"
#include "Common/Parser.h"

namespace mediakit {

int RtpPayload::getClockRate(int pt) {
    switch (pt) {
#define SWITCH_CASE(name, type, value, clock_rate, channel, codec_id) case value :  return clock_rate;
        RTP_PT_MAP(SWITCH_CASE)
#undef SWITCH_CASE
        default: return 90000;
    }
}

TrackType RtpPayload::getTrackType(int pt) {
    switch (pt) {
#define SWITCH_CASE(name, type, value, clock_rate, channel, codec_id) case value :  return type;
        RTP_PT_MAP(SWITCH_CASE)
#undef SWITCH_CASE
        default: return TrackInvalid;
    }
}

int RtpPayload::getAudioChannel(int pt) {
    switch (pt) {
#define SWITCH_CASE(name, type, value, clock_rate, channel, codec_id) case value :  return channel;
        RTP_PT_MAP(SWITCH_CASE)
#undef SWITCH_CASE
        default: return 1;
    }
}

const char *RtpPayload::getName(int pt) {
    switch (pt) {
#define SWITCH_CASE(name, type, value, clock_rate, channel, codec_id) case value :  return #name;
        RTP_PT_MAP(SWITCH_CASE)
#undef SWITCH_CASE
        default: return "unknown payload type";
    }
}

CodecId RtpPayload::getCodecId(int pt) {
    switch (pt) {
#define SWITCH_CASE(name, type, value, clock_rate, channel, codec_id) case value :  return codec_id;
        RTP_PT_MAP(SWITCH_CASE)
#undef SWITCH_CASE
        default: return CodecInvalid;
    }
}

static void getAttrSdp(const multimap<string, string> &attr, _StrPrinter &printer) {
    const map<string, string>::value_type *ptr = nullptr;
    for (auto &pr : attr) {
        if (pr.first == "control") {
            ptr = &pr;
            continue;
        }
        if (pr.second.empty()) {
            printer << "a=" << pr.first << "\r\n";
        } else {
            printer << "a=" << pr.first << ":" << pr.second << "\r\n";
        }
    }
    if (ptr) {
        printer << "a=" << ptr->first << ":" << ptr->second << "\r\n";
    }
}

string SdpTrack::getName() const {
    switch (_pt) {
#define SWITCH_CASE(name, type, value, clock_rate, channel, codec_id) case value :  return #name;
        RTP_PT_MAP(SWITCH_CASE)
#undef SWITCH_CASE
        default: return _codec;
    }
}

string SdpTrack::getControlUrl(const string &base_url) const {
    if (_control.find("://") != string::npos) {
        //以rtsp://开头
        return _control;
    }
    return base_url + "/" + _control;
}

string SdpTrack::toString(uint16_t port) const {
    _StrPrinter _printer;
    switch (_type) {
        case TrackTitle: {
            TitleSdp title(atof(_t.data()));
            _printer << title.getSdp();
            getAttrSdp(_attr, _printer);
            break;
        }
        case TrackAudio:
        case TrackVideo: {
            if (_type == TrackAudio) {
                _printer << "m=audio " << port << " RTP/AVP " << _pt << "\r\n";
            } else {
                _printer << "m=video " << port << " RTP/AVP " << _pt << "\r\n";
            }
            if (!_b.empty()) {
                _printer << "b=" << _b << "\r\n";
            }
            getAttrSdp(_attr, _printer);
            break;
        }
        default: break;
    }
    return std::move(_printer);
}

static TrackType toTrackType(const string &str) {
    if (str == "") {
        return TrackTitle;
    }

    if (str == "video") {
        return TrackVideo;
    }

    if (str == "audio") {
        return TrackAudio;
    }

    return TrackInvalid;
}

void SdpParser::load(const string &sdp) {
    {
        _track_vec.clear();
        SdpTrack::Ptr track = std::make_shared<SdpTrack>();
        track->_type = TrackTitle;
        _track_vec.emplace_back(track);

        auto lines = split(sdp, "\n");
        for (auto &line : lines) {
            trim(line);
            if (line.size() < 2 || line[1] != '=') {
                continue;
            }
            char opt = line[0];
            string opt_val = line.substr(2);
            switch (opt) {
                case 't':
                    track->_t = opt_val;
                    break;
                case 'b':
                    track->_b = opt_val;
                    break;
                case 'm': {
                    track = std::make_shared<SdpTrack>();
                    int pt, port;
                    char rtp[16] = {0}, type[16];
                    if (4 == sscanf(opt_val.data(), " %15[^ ] %d %15[^ ] %d", type, &port, rtp, &pt)) {
                        track->_pt = pt;
                        track->_samplerate = RtpPayload::getClockRate(pt);
                        track->_channel = RtpPayload::getAudioChannel(pt);
                        track->_type = toTrackType(type);
                        track->_port = port;
                        _track_vec.emplace_back(track);
                    }
                    break;
                }
                case 'a': {
                    string attr = FindField(opt_val.data(), nullptr, ":");
                    if (attr.empty()) {
                        track->_attr.emplace(opt_val, "");
                    } else {
                        track->_attr.emplace(attr, FindField(opt_val.data(), ":", nullptr));
                    }
                    break;
                }
                default: track->_other[opt] = opt_val; break;
            }
        }
    }

    for (auto &track_ptr : _track_vec) {
        auto &track = *track_ptr;
        auto it = track._attr.find("range");
        if (it != track._attr.end()) {
            char name[16] = {0}, start[16] = {0}, end[16] = {0};
            int ret = sscanf(it->second.data(), "%15[^=]=%15[^-]-%15s", name, start, end);
            if (3 == ret || 2 == ret) {
                if (strcmp(start, "now") == 0) {
                    strcpy(start, "0");
                }
                track._start = (float) atof(start);
                track._end = (float) atof(end);
                track._duration = track._end - track._start;
            }
        }

        for (it = track._attr.find("rtpmap"); it != track._attr.end() && it->first == "rtpmap";) {
            auto &rtpmap = it->second;
            int pt, samplerate, channel;
            char codec[16] = {0};

            sscanf(rtpmap.data(), "%d", &pt);
            if (track._pt != pt) {
                //pt不匹配
                it = track._attr.erase(it);
                continue;
            }
            if (4 == sscanf(rtpmap.data(), "%d %15[^/]/%d/%d", &pt, codec, &samplerate, &channel)) {
                track._codec = codec;
                track._samplerate = samplerate;
                track._channel = channel;
            } else if (3 == sscanf(rtpmap.data(), "%d %15[^/]/%d", &pt, codec, &samplerate)) {
                track._pt = pt;
                track._codec = codec;
                track._samplerate = samplerate;
            }
            if (!track._samplerate && track._type == TrackVideo) {
                //未设置视频采样率时，赋值为90000
                track._samplerate = 90000;
            }
            ++it;
        }

        for (it = track._attr.find("fmtp"); it != track._attr.end() && it->first == "fmtp"; ) {
            auto &fmtp = it->second;
            int pt;
            sscanf(fmtp.data(), "%d", &pt);
            if (track._pt != pt) {
                //pt不匹配
                it = track._attr.erase(it);
                continue;
            }
            track._fmtp = FindField(fmtp.data(), " ", nullptr);
            ++it;
        }

        it = track._attr.find("control");
        if (it != track._attr.end()) {
            track._control = it->second;
        }
    }
}

bool SdpParser::available() const {
    return getTrack(TrackAudio) || getTrack(TrackVideo);
}

SdpTrack::Ptr SdpParser::getTrack(TrackType type) const {
    for (auto &track : _track_vec) {
        if (track->_type == type) {
            return track;
        }
    }
    return nullptr;
}

vector<SdpTrack::Ptr> SdpParser::getAvailableTrack() const {
    vector<SdpTrack::Ptr> ret;
    bool audio_added = false;
    bool video_added = false;
    for (auto &track : _track_vec) {
        if (track->_type == TrackAudio) {
            if (!audio_added) {
                ret.emplace_back(track);
                audio_added = true;
            }
            continue;
        }

        if (track->_type == TrackVideo) {
            if (!video_added) {
                ret.emplace_back(track);
                video_added = true;
            }
            continue;
        }
    }
    return ret;
}

string SdpParser::toString() const {
    string title, audio, video;
    for (auto &track : _track_vec) {
        switch (track->_type) {
            case TrackTitle: {
                title = track->toString();
                break;
            }
            case TrackVideo: {
                video = track->toString();
                break;
            }
            case TrackAudio: {
                audio = track->toString();
                break;
            }
            default: break;
        }
    }
    return title + video + audio;
}

bool RtspUrl::parse(const string &strUrl) {
    auto schema = FindField(strUrl.data(), nullptr, "://");
    bool isSSL = strcasecmp(schema.data(), "rtsps") == 0;
    //查找"://"与"/"之间的字符串，用于提取用户名密码
    auto middle_url = FindField(strUrl.data(), "://", "/");
    if (middle_url.empty()) {
        middle_url = FindField(strUrl.data(), "://", nullptr);
    }
    auto pos = middle_url.rfind('@');
    if (pos == string::npos) {
        //并没有用户名密码
        return setup(isSSL, strUrl, "", "");
    }

    //包含用户名密码
    auto user_pwd = middle_url.substr(0, pos);
    auto suffix = strUrl.substr(schema.size() + 3 + pos + 1);
    auto url = StrPrinter << "rtsp://" << suffix << endl;
    if (user_pwd.find(":") == string::npos) {
        return setup(isSSL, url, user_pwd, "");
    }
    auto user = FindField(user_pwd.data(), nullptr, ":");
    auto pwd = FindField(user_pwd.data(), ":", nullptr);
    return setup(isSSL, url, user, pwd);
}

bool RtspUrl::setup(bool isSSL, const string &strUrl, const string &strUser, const string &strPwd) {
    auto ip = FindField(strUrl.data(), "://", "/");
    if (ip.empty()) {
        ip = split(FindField(strUrl.data(), "://", NULL), "?")[0];
    }
    auto port = atoi(FindField(ip.data(), ":", NULL).data());
    if (port <= 0 || port > UINT16_MAX) {
        //rtsp 默认端口554
        port = isSSL ? 322 : 554;
    } else {
        //服务器域名
        ip = FindField(ip.data(), NULL, ":");
    }

    if (ip.empty()) {
        return false;
    }

    _url = std::move(strUrl);
    _user = std::move(strUser);
    _passwd = std::move(strPwd);
    _host = std::move(ip);
    _port = port;
    _is_ssl = isSSL;
    return true;
}

static void makeSockPair_l(std::pair<Socket::Ptr, Socket::Ptr> &pair, const string &local_ip) {
    auto &pSockRtp = pair.first;
    auto &pSockRtcp = pair.second;

    if (!pSockRtp->bindUdpSock(0, local_ip.data())) {
        //分配端口失败
        throw runtime_error("open udp socket failed");
    }

    //是否是偶数
    bool even_numbers = pSockRtp->get_local_port() % 2 == 0;
    if (!pSockRtcp->bindUdpSock(pSockRtp->get_local_port() + (even_numbers ? 1 : -1), local_ip.data())) {
        //分配端口失败
        throw runtime_error("open udp socket failed");
    }

    if (!even_numbers) {
        //如果rtp端口不是偶数，那么与rtcp端口互换，目的是兼容一些要求严格的播放器或服务器
        Socket::Ptr tmp = pSockRtp;
        pSockRtp = pSockRtcp;
        pSockRtcp = tmp;
    }
}

void makeSockPair(std::pair<Socket::Ptr, Socket::Ptr> &pair, const string &local_ip) {
    int try_count = 0;
    while (true) {
        try {
            makeSockPair_l(pair, local_ip);
            break;
        } catch (...) {
            if (++try_count == 3) {
                throw;
            }
            WarnL << "open udp socket failed, retry: " << try_count;
        }
    }
}

string printSSRC(uint32_t ui32Ssrc) {
    char tmp[9] = {0};
    ui32Ssrc = htonl(ui32Ssrc);
    uint8_t *pSsrc = (uint8_t *) &ui32Ssrc;
    for (int i = 0; i < 4; i++) {
        sprintf(tmp + 2 * i, "%02X", pSsrc[i]);
    }
    return tmp;
}

Buffer::Ptr makeRtpOverTcpPrefix(uint16_t size, uint8_t interleaved) {
    auto rtp_tcp = BufferRaw::create();
    rtp_tcp->setCapacity(RtpPacket::kRtpTcpHeaderSize);
    rtp_tcp->setSize(RtpPacket::kRtpTcpHeaderSize);
    auto ptr = rtp_tcp->data();
    ptr[0] = '$';
    ptr[1] = interleaved;
    ptr[2] = (size >> 8) & 0xFF;
    ptr[3] = size & 0xFF;
    return rtp_tcp;
}

#define AV_RB16(x)                           \
    ((((const uint8_t*)(x))[0] << 8) |          \
      ((const uint8_t*)(x))[1])

size_t RtpHeader::getCsrcSize() const {
    //每个csrc占用4字节
    return csrc << 2;
}

uint8_t *RtpHeader::getCsrcData() {
    if (!csrc) {
        return nullptr;
    }
    return &payload;
}

size_t RtpHeader::getExtSize() const {
    //rtp有ext
    if (!ext) {
        return 0;
    }
    auto ext_ptr = &payload + getCsrcSize();
    //uint16_t reserved = AV_RB16(ext_ptr);
    //每个ext占用4字节
    return AV_RB16(ext_ptr + 2) << 2;
}

uint16_t RtpHeader::getExtReserved() const {
    //rtp有ext
    if (!ext) {
        return 0;
    }
    auto ext_ptr = &payload + getCsrcSize();
    return AV_RB16(ext_ptr);
}

uint8_t *RtpHeader::getExtData() {
    if (!ext) {
        return nullptr;
    }
    auto ext_ptr = &payload + getCsrcSize();
    //多出的4个字节分别为reserved、ext_len
    return ext_ptr + 4;
}

size_t RtpHeader::getPayloadOffset() const {
    //有ext时，还需要忽略reserved、ext_len 4个字节
    return getCsrcSize() + (ext ? (4 + getExtSize()) : 0);
}

uint8_t *RtpHeader::getPayloadData() {
    return &payload + getPayloadOffset();
}

size_t RtpHeader::getPaddingSize(size_t rtp_size) const {
    if (!padding) {
        return 0;
    }
    auto end = (uint8_t *) this + rtp_size - 1;
    return *end;
}

size_t RtpHeader::getPayloadSize(size_t rtp_size) const {
    auto invalid_size = getPayloadOffset() + getPaddingSize(rtp_size);
    if (invalid_size + RtpPacket::kRtpHeaderSize >= rtp_size) {
        return 0;
    }
    return rtp_size - invalid_size - RtpPacket::kRtpHeaderSize;
}

string RtpHeader::dumpString(size_t rtp_size) const {
    _StrPrinter printer;
    printer << "version:" << (int) version << "\r\n";
    printer << "padding:" << getPaddingSize(rtp_size) << "\r\n";
    printer << "ext:" << getExtSize() << "\r\n";
    printer << "csrc:" << getCsrcSize() << "\r\n";
    printer << "mark:" << (int) mark << "\r\n";
    printer << "pt:" << (int) pt << "\r\n";
    printer << "seq:" << ntohs(seq) << "\r\n";
    printer << "stamp:" << ntohl(stamp) << "\r\n";
    printer << "ssrc:" << ntohl(ssrc) << "\r\n";
    printer << "rtp size:" << rtp_size << "\r\n";
    printer << "payload offset:" << getPayloadOffset() << "\r\n";
    printer << "payload size:" << getPayloadSize(rtp_size) << "\r\n";
    return std::move(printer);
}

///////////////////////////////////////////////////////////////////////

RtpHeader *RtpPacket::getHeader() {
    //需除去rtcp over tcp 4个字节长度
    return (RtpHeader *) (data() + RtpPacket::kRtpTcpHeaderSize);
}

const RtpHeader *RtpPacket::getHeader() const {
    return (RtpHeader *) (data() + RtpPacket::kRtpTcpHeaderSize);
}

string RtpPacket::dumpString() const {
    return ((RtpPacket *) this)->getHeader()->dumpString(size() - RtpPacket::kRtpTcpHeaderSize);
}

uint16_t RtpPacket::getSeq() const {
    return ntohs(getHeader()->seq);
}

uint32_t RtpPacket::getStamp() const {
    return ntohl(getHeader()->stamp);
}

uint32_t RtpPacket::getStampMS() const {
    return ntp_stamp & 0xFFFFFFFF;
}

uint32_t RtpPacket::getSSRC() const {
    return ntohl(getHeader()->ssrc);
}

uint8_t *RtpPacket::getPayload() {
    return getHeader()->getPayloadData();
}

size_t RtpPacket::getPayloadSize() const {
    //需除去rtcp over tcp 4个字节长度
    return getHeader()->getPayloadSize(size() - kRtpTcpHeaderSize);
}

RtpPacket::Ptr RtpPacket::create() {
#if 0
    static ResourcePool<RtpPacket> packet_pool;
    static onceToken token([]() {
        packet_pool.setSize(1024);
    });
    auto ret = packet_pool.obtain();
    ret->setSize(0);
    return ret;
#else
    return Ptr(new RtpPacket);
#endif
}

}//namespace mediakit

namespace toolkit {
StatisticImp(mediakit::RtpPacket);
}