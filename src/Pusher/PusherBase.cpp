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

using namespace toolkit;

namespace mediakit {

PusherBase::Ptr PusherBase::createPusher(const EventPoller::Ptr &in_poller,
                                         const MediaSource::Ptr &src,
                                         const std::string & url) {
    auto poller = in_poller ? in_poller : EventPollerPool::Instance().getPoller();
    std::weak_ptr<EventPoller> weak_poller = poller;
    static auto release_func = [weak_poller](PusherBase *ptr) {
        if (auto poller = weak_poller.lock()) {
            poller->async([ptr]() {
                onceToken token(nullptr, [&]() { delete ptr; });
                ptr->teardown();
            });
        } else {
            delete ptr;
        }
    };
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

    throw std::invalid_argument("not supported push schema:" + url);
}

PusherBase::PusherBase() {
    this->mINI::operator[](Client::kTimeoutMS) = 10000;
    this->mINI::operator[](Client::kBeatIntervalMS) = 5000;
}

} /* namespace mediakit */
