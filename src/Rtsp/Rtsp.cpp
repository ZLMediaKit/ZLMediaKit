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

#include <stdlib.h>
#include "Rtsp.h"
#include "Common/Parser.h"

namespace mediakit{

int RtpPayload::getClockRate(int pt){
    switch (pt){
#define SWITCH_CASE(name, type, value, clock_rate, channel) case value :  return clock_rate;
        RTP_PT_MAP(SWITCH_CASE)
#undef SWITCH_CASE
        default: return 90000;
    }
}

TrackType RtpPayload::getTrackType(int pt){
    switch (pt){
#define SWITCH_CASE(name, type, value, clock_rate, channel) case value :  return type;
        RTP_PT_MAP(SWITCH_CASE)
#undef SWITCH_CASE
        default: return TrackInvalid;
    }
}

int RtpPayload::getAudioChannel(int pt){
    switch (pt){
#define SWITCH_CASE(name, type, value, clock_rate, channel) case value :  return channel;
        RTP_PT_MAP(SWITCH_CASE)
#undef SWITCH_CASE
        default: return 1;
    }
}

const char * RtpPayload::getName(int pt){
    switch (pt){
#define SWITCH_CASE(name, type, value, clock_rate, channel) case value :  return #name;
        RTP_PT_MAP(SWITCH_CASE)
#undef SWITCH_CASE
        default: return "unknown payload type";
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
#define SWITCH_CASE(name, type, value, clock_rate, channel) case value :  return #name;
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

            _printer << "s=RTSP Session, streamed by the ZLMediaKit\r\n";
            _printer << "i=ZLMediaKit Live Stream\r\n";
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
            int pt, samplerate;
            char codec[16] = {0};
            if (3 == sscanf(rtpmap.data(), "%d %15[^/]/%d", &pt, codec, &samplerate)) {
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

}//namespace mediakit

