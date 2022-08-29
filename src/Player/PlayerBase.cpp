/*
 * Copyright (c) 2016 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/xia-chu/ZLMediaKit).
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
#include "Http/TsPlayerImp.h"

using namespace std;
using namespace toolkit;

namespace mediakit {

PlayerBase::Ptr PlayerBase::createPlayer(const EventPoller::Ptr &poller, const string &url_in) {
    static auto releasePlayer = [](PlayerBase *ptr) {
        onceToken token(nullptr, [&]() {
            delete ptr;
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

    if (strcasecmp("rtsps", prefix.data()) == 0) {
        return PlayerBase::Ptr(new TcpClientWithSSL<RtspPlayerImp>(poller), releasePlayer);
    }

    if (strcasecmp("rtsp", prefix.data()) == 0) {
        return PlayerBase::Ptr(new RtspPlayerImp(poller), releasePlayer);
    }

    if (strcasecmp("rtmps", prefix.data()) == 0) {
        return PlayerBase::Ptr(new TcpClientWithSSL<RtmpPlayerImp>(poller), releasePlayer);
    }

    if (strcasecmp("rtmp", prefix.data()) == 0) {
        return PlayerBase::Ptr(new RtmpPlayerImp(poller), releasePlayer);
    }
    if ((strcasecmp("http", prefix.data()) == 0 || strcasecmp("https", prefix.data()) == 0)) {
        if (end_with(url, ".m3u8") || end_with(url_in, ".m3u8")) {
            return PlayerBase::Ptr(new HlsPlayerImp(poller), releasePlayer);
        } else if (end_with(url, ".ts") || end_with(url_in, ".ts")) {
            return PlayerBase::Ptr(new TsPlayerImp(poller), releasePlayer);
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
