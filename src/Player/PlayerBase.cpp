/*
 * Copyright (c) 2016 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/xiongziliang/ZLMediaKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#include <algorithm>
#include "PlayerBase.h"
#include "Rtsp/RtspPlayerImp.h"
#include "Rtmp/RtmpPlayerImp.h"
#include "Http/HlsPlayer.h"
using namespace toolkit;

namespace mediakit {

//字符串是否以xx结尾
static bool end_of(const string &str, const string &substr){
    auto pos = str.rfind(substr);
    return pos != string::npos && pos == str.size() - substr.size();
}

PlayerBase::Ptr PlayerBase::createPlayer(const EventPoller::Ptr &poller,const string &url_in) {
    static auto releasePlayer = [](PlayerBase *ptr){
        onceToken token(nullptr,[&](){
            delete  ptr;
        });
        ptr->teardown();
    };
    string url = url_in;
    string prefix = FindField(url.data(), NULL, "://");
    auto pos = url.find('?');
    if (pos != string::npos) {
        //去除？后面的字符串
        url = url.substr(0, pos);
    }

    if (strcasecmp("rtsps",prefix.data()) == 0) {
        return PlayerBase::Ptr(new TcpClientWithSSL<RtspPlayerImp>(poller),releasePlayer);
    }

    if (strcasecmp("rtsp",prefix.data()) == 0) {
        return PlayerBase::Ptr(new RtspPlayerImp(poller),releasePlayer);
    }

    if (strcasecmp("rtmps",prefix.data()) == 0) {
        return PlayerBase::Ptr(new TcpClientWithSSL<RtmpPlayerImp>(poller),releasePlayer);
    }

    if (strcasecmp("rtmp",prefix.data()) == 0) {
        return PlayerBase::Ptr(new RtmpPlayerImp(poller),releasePlayer);
    }

    if ((strcasecmp("http",prefix.data()) == 0 || strcasecmp("https",prefix.data()) == 0) && end_of(url, ".m3u8")) {
        return PlayerBase::Ptr(new HlsPlayerImp(poller),releasePlayer);
    }

    return PlayerBase::Ptr(new RtspPlayerImp(poller),releasePlayer);
}

PlayerBase::PlayerBase() {
    this->mINI::operator[](kTimeoutMS) = 10000;
    this->mINI::operator[](kMediaTimeoutMS) = 5000;
    this->mINI::operator[](kBeatIntervalMS) = 5000;
    this->mINI::operator[](kMaxAnalysisMS) = 5000;
}

///////////////////////////Demuxer//////////////////////////////
bool Demuxer::isInited(int analysisMs) {
    if(analysisMs && _ticker.createdTime() > analysisMs){
        //analysisMs毫秒后强制初始化完毕
        return true;
    }
    if (_videoTrack && !_videoTrack->ready()) {
        //视频未准备好
        return false;
    }
    if (_audioTrack && !_audioTrack->ready()) {
        //音频未准备好
        return false;
    }
    return true;
}

vector<Track::Ptr> Demuxer::getTracks(bool trackReady) const {
    vector<Track::Ptr> ret;
    if(_videoTrack){
        if(trackReady){
            if(_videoTrack->ready()){
                ret.emplace_back(_videoTrack);
            }
        }else{
            ret.emplace_back(_videoTrack);
        }
    }
    if(_audioTrack){
        if(trackReady){
            if(_audioTrack->ready()){
                ret.emplace_back(_audioTrack);
            }
        }else{
            ret.emplace_back(_audioTrack);
        }
    }
    return std::move(ret);
}

float Demuxer::getDuration() const {
    return _fDuration;
}

void Demuxer::onAddTrack(const Track::Ptr &track){
    if(_listener){
        _listener->onAddTrack(track);
    }
}

void Demuxer::setTrackListener(Demuxer::Listener *listener) {
    _listener = listener;
}

} /* namespace mediakit */
