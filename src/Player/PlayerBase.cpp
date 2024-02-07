/*
 * Copyright (c) 2016-present The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/ZLMediaKit/ZLMediaKit).
 *
 * Use of this source code is governed by MIT-like license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#include <algorithm>
#include "PlayerBase.h"
#include "Rtsp/RtspPlayerImp.h"
#include "Rtmp/RtmpPlayerImp.h"
#include "Rtmp/FlvPlayer.h"
#include "Http/HlsPlayer.h"
#include "Http/TsPlayerImp.h"

using namespace std;
using namespace toolkit;

namespace mediakit {

PlayerBase::Ptr PlayerBase::createPlayer(const EventPoller::Ptr &in_poller, const string &url_in) {
    auto poller = in_poller ? in_poller : EventPollerPool::Instance().getPoller();
    std::weak_ptr<EventPoller> weak_poller = poller;
    static auto release_func = [weak_poller](PlayerBase *ptr) {
        if (auto poller = weak_poller.lock()) {
            poller->async([ptr]() {
                onceToken token(nullptr, [&]() { delete ptr; });
                ptr->teardown();
            });
        } else {
            delete ptr;
        }
    };
    string url = url_in;
    string prefix = findSubString(url.data(), NULL, "://");
    auto pos = url.find('?');
    if (pos != string::npos) {
        //去除？后面的字符串
        url = url.substr(0, pos);
    }

    if (strcasecmp("rtsps", prefix.data()) == 0) {
        return PlayerBase::Ptr(new TcpClientWithSSL<RtspPlayerImp>(poller), release_func);
    }

    if (strcasecmp("rtsp", prefix.data()) == 0) {
        return PlayerBase::Ptr(new RtspPlayerImp(poller), release_func);
    }

    if (strcasecmp("rtmps", prefix.data()) == 0) {
        return PlayerBase::Ptr(new TcpClientWithSSL<RtmpPlayerImp>(poller), release_func);
    }

    if (strcasecmp("rtmp", prefix.data()) == 0) {
        return PlayerBase::Ptr(new RtmpPlayerImp(poller), release_func);
    }
    if ((strcasecmp("http", prefix.data()) == 0 || strcasecmp("https", prefix.data()) == 0)) {
        if (end_with(url, ".m3u8") || end_with(url_in, ".m3u8")) {
            return PlayerBase::Ptr(new HlsPlayerImp(poller), release_func);
        }
        if (end_with(url, ".ts") || end_with(url_in, ".ts")) {
            return PlayerBase::Ptr(new TsPlayerImp(poller), release_func);
        }
        if (end_with(url, ".flv") || end_with(url_in, ".flv")) {
            return PlayerBase::Ptr(new FlvPlayerImp(poller), release_func);
        }
    }

    throw std::invalid_argument("not supported play schema:" + url_in);
}

PlayerBase::PlayerBase() {
    this->mINI::operator[](Client::kTimeoutMS) = 10000;
    this->mINI::operator[](Client::kMediaTimeoutMS) = 5000;
    this->mINI::operator[](Client::kBeatIntervalMS) = 5000;
    this->mINI::operator[](Client::kWaitTrackReady) = true;
}

} /* namespace mediakit */
