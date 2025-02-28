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
#include "PusherBase.h"
#include "Rtsp/RtspPusher.h"
#include "Rtmp/RtmpPusher.h"
#ifdef ENABLE_SRT
#include "Srt/SrtPusher.h"
#endif // ENABLE_SRT

using namespace toolkit;

namespace mediakit {

static bool checkMediaSourceAndUrlMatch(const MediaSource::Ptr &src, const std::string &url) {
    std::string prefix = findSubString(url.data(), NULL, "://");

    if (strcasecmp("rtsps", prefix.data()) == 0 || strcasecmp("rtsp", prefix.data()) == 0) {
        auto rtsp_src = std::dynamic_pointer_cast<RtspMediaSource>(src);
        if (!rtsp_src) {
            return false;
        }
    }

    if (strcasecmp("rtmp", prefix.data()) == 0 || strcasecmp("rtmps", prefix.data()) == 0) {
        auto rtmp_src = std::dynamic_pointer_cast<RtmpMediaSource>(src);
        if (!rtmp_src) {
            return false;
        }
    }

#ifdef ENABLE_SRT
    if (strcasecmp("srt", prefix.data()) == 0) {
        auto ts_src = std::dynamic_pointer_cast<TSMediaSource>(src);
        if (!ts_src) {
            return false;
        }
    }
#endif // ENABLE_SRT
    return true;
}

PusherBase::Ptr PusherBase::createPusher(const EventPoller::Ptr &in_poller,
                                         const MediaSource::Ptr &src,
                                         const std::string & url) {
    auto poller = in_poller ? in_poller : EventPollerPool::Instance().getPoller();
    std::weak_ptr<EventPoller> weak_poller = poller;
    auto release_func = [weak_poller](PusherBase *ptr) {
        if (auto poller = weak_poller.lock()) {
            poller->async([ptr]() {
                onceToken token(nullptr, [&]() { delete ptr; });
                ptr->teardown();
            });
        } else {
            delete ptr;
        }
    };
    if (!checkMediaSourceAndUrlMatch(src, url)) {
        throw std::invalid_argument(" media source (schema) and  push url not match");
    }

    std::string prefix = findSubString(url.data(), NULL, "://");

    if (strcasecmp("rtsps",prefix.data()) == 0) {
        return PusherBase::Ptr(new TcpClientWithSSL<RtspPusherImp>(poller, std::dynamic_pointer_cast<RtspMediaSource>(src)), release_func);
    }

    if (strcasecmp("rtsp",prefix.data()) == 0) {
        return PusherBase::Ptr(new RtspPusherImp(poller, std::dynamic_pointer_cast<RtspMediaSource>(src)), release_func);
    }

    if (strcasecmp("rtmps",prefix.data()) == 0) {
        return PusherBase::Ptr(new TcpClientWithSSL<RtmpPusherImp>(poller, std::dynamic_pointer_cast<RtmpMediaSource>(src)), release_func);
    }

    if (strcasecmp("rtmp",prefix.data()) == 0) {
        return PusherBase::Ptr(new RtmpPusherImp(poller, std::dynamic_pointer_cast<RtmpMediaSource>(src)), release_func);
    }

#ifdef ENABLE_SRT
    if (strcasecmp("srt", prefix.data()) == 0) {
        return PusherBase::Ptr(new SrtPusherImp(poller, std::dynamic_pointer_cast<TSMediaSource>(src)), release_func);
    }
#endif//ENABLE_SRT


    throw std::invalid_argument("not supported push schema:" + url);
}

PusherBase::PusherBase() {
    this->mINI::operator[](Client::kTimeoutMS) = 10000;
    this->mINI::operator[](Client::kBeatIntervalMS) = 5000;
}

} /* namespace mediakit */
