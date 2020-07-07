/*
 * Copyright (c) 2016 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/xiongziliang/ZLMediaKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#include <stdlib.h>
#include "Rtsp.h"
#include "Common/Parser.h"

namespace mediakit{

int RtpPayload::getClockRate(int pt){
    switch (pt){
#define SWITCH_CASE(name, type, value, clock_rate, channel, codec_id) case value :  return clock_rate;
        RTP_PT_MAP(SWITCH_CASE)
#undef SWITCH_CASE
        default: return 90000;
    }
}

TrackType RtpPayload::getTrackType(int pt){
    switch (pt){
#define SWITCH_CASE(name, type, value, clock_rate, channel, codec_id) case value :  return type;
        RTP_PT_MAP(SWITCH_CASE)
#undef SWITCH_CASE
        default: return TrackInvalid;
    }
}

int RtpPayload::getAudioChannel(int pt){
    switch (pt){
#define SWITCH_CASE(name, type, value, clock_rate, channel, codec_id) case value :  return channel;
        RTP_PT_MAP(SWITCH_CASE)
#undef SWITCH_CASE
        default: return 1;
    }
}

const char * RtpPayload::getName(int pt){
    switch (pt){
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
        default : return CodecInvalid;
    }
}

static void getAttrSdp(const map<string, string> &attr, _StrPrinter &printer){
    const map<string, string>::value_type *ptr = nullptr;
    for(auto &pr : attr){
        if(pr.first == "control"){
            ptr = &pr;
            continue;
        }
        if(pr.second.empty()){
            printer << "a=" << pr.first << "\r\n";
        }else{
            printer << "a=" << pr.first << ":" << pr.second << "\r\n";
        }
    }
    if(ptr){
        printer << "a=" << ptr->first << ":" << ptr->second << "\r\n";
    }
}

string SdpTrack::getName() const{
    switch (_pt){
#define SWITCH_CASE(name, type, value, clock_rate, channel, codec_id) case value :  return #name;
        RTP_PT_MAP(SWITCH_CASE)
#undef SWITCH_CASE
        default: return _codec;
    }
}

string SdpTrack::toString() const {
    _StrPrinter _printer;
    switch (_type){
        case TrackTitle:{
            _printer << "v=" << 0 << "\r\n";
            if(!_o.empty()){
                _printer << "o="<< _o << "\r\n";
            }
            if(!_c.empty()){
                _printer << "c=" << _c << "\r\n";
            }
            if(!_t.empty()){
                _printer << "t=" << _t << "\r\n";
            }

            _printer << "s=Streamed by " << SERVER_NAME << "\r\n";
            getAttrSdp(_attr,_printer);
        }
            break;
        case TrackAudio:
        case TrackVideo:{
            if(_type == TrackAudio){
                _printer << "m=audio 0 RTP/AVP " << _pt << "\r\n";
            }else{
                _printer << "m=video 0 RTP/AVP " << _pt << "\r\n";
            }
            if(!_b.empty()){
                _printer << "b=" <<_b << "\r\n";
            }
            getAttrSdp(_attr,_printer);
        }
            break;
        default:
            break;
    }
    return _printer;
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
                case 'o':
                    track->_o = opt_val;
                    break;
                case 's':
                    track->_s = opt_val;
                    break;
                case 'i':
                    track->_i = opt_val;
                    break;
                case 'c':
                    track->_c = opt_val;
                    break;
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
                        track->_samplerate = RtpPayload::getClockRate(pt) ;
                        track->_channel = RtpPayload::getAudioChannel(pt);
                        track->_type = toTrackType(type);
                        track->_m = opt_val;
                        track->_port = port;
                        _track_vec.emplace_back(track);
                    }
                }
                    break;
                case 'a': {
                    string attr = FindField(opt_val.data(), nullptr, ":");
                    if (attr.empty()) {
                        track->_attr[opt_val] = "";
                    } else {
                        track->_attr[attr] = FindField(opt_val.data(), ":", nullptr);
                    }
                }
                    break;
                default:
                    track->_other[opt] = opt_val;
                    break;
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
                track._start = atof(start);
                track._end = atof(end);
                track._duration = track._end - track._start;
            }
        }

        it = track._attr.find("rtpmap");
        if(it != track._attr.end()){
            auto rtpmap = it->second;
            int pt, samplerate, channel;
            char codec[16] = {0};
            if (4 == sscanf(rtpmap.data(), "%d %15[^/]/%d/%d", &pt, codec, &samplerate, &channel)) {
                track._pt = pt;
                track._codec = codec;
                track._samplerate = samplerate;
                track._channel = channel;
            }else if (3 == sscanf(rtpmap.data(), "%d %15[^/]/%d", &pt, codec, &samplerate)) {
                track._pt = pt;
                track._codec = codec;
                track._samplerate = samplerate;
            }
        }

        it = track._attr.find("fmtp");
        if(it != track._attr.end()) {
            track._fmtp = it->second;
        }

        it = track._attr.find("control");
        if(it != track._attr.end()) {
            track._control = it->second;
            auto surffix = string("/") + track._control;
            track._control_surffix = surffix.substr(1 + surffix.rfind('/'));
        }
    }
}

bool SdpParser::available() const {
    return getTrack(TrackAudio) || getTrack(TrackVideo);
}

SdpTrack::Ptr SdpParser::getTrack(TrackType type) const {
    for (auto &track : _track_vec){
        if(track->_type == type){
            return track;
        }
    }
    return nullptr;
}

vector<SdpTrack::Ptr> SdpParser::getAvailableTrack() const {
    vector<SdpTrack::Ptr> ret;
    bool audio_added = false;
    bool video_added = false;
    for (auto &track : _track_vec){
        if(track->_type == TrackAudio ){
            if(!audio_added){
                ret.emplace_back(track);
                audio_added = true;
            }
            continue;
        }

        if(track->_type == TrackVideo ){
            if(!video_added){
                ret.emplace_back(track);
                video_added = true;
            }
            continue;
        }
    }
    return std::move(ret);
}

string SdpParser::toString() const {
    string title,audio,video;
    for(auto &track : _track_vec){
        switch (track->_type){
            case TrackTitle:{
                title = track->toString();
            }
                break;
            case TrackVideo:{
                video = track->toString();
            }
                break;
            case TrackAudio:{
                audio = track->toString();
            }
                break;
            default:
                break;
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

std::pair<Socket::Ptr, Socket::Ptr> makeSockPair_l(const EventPoller::Ptr &poller, const string &local_ip){
    auto pSockRtp = std::make_shared<Socket>(poller);
    if (!pSockRtp->bindUdpSock(0, local_ip.data())) {
        //分配端口失败
        throw runtime_error("open udp socket failed");
    }

    //是否是偶数
    bool even_numbers = pSockRtp->get_local_port() % 2 == 0;
    auto pSockRtcp = std::make_shared<Socket>(poller);
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

    return std::make_pair(pSockRtp, pSockRtcp);
}

std::pair<Socket::Ptr, Socket::Ptr> makeSockPair(const EventPoller::Ptr &poller, const string &local_ip){
    int try_count = 0;
    while (true) {
        try {
            return makeSockPair_l(poller, local_ip);
        } catch (...) {
            if (++try_count == 3) {
                throw;
            }
            WarnL << "open udp socket failed, retry: " << try_count;
        }
    }
}

string printSSRC(uint32_t ui32Ssrc) {
    char tmp[9] = { 0 };
    ui32Ssrc = htonl(ui32Ssrc);
    uint8_t *pSsrc = (uint8_t *) &ui32Ssrc;
    for (int i = 0; i < 4; i++) {
        sprintf(tmp + 2 * i, "%02X", pSsrc[i]);
    }
    return tmp;
}

}//namespace mediakit