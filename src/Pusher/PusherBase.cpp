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
#include "PusherBase.h"
#include "Rtsp/RtspPusher.h"
#include "Rtmp/RtmpPusher.h"

using namespace toolkit;

namespace mediakit {

PusherBase::Ptr PusherBase::createPusher(const EventPoller::Ptr &poller,
                                         const MediaSource::Ptr &src,
                                         const std::string & url) {
    static auto releasePusher = [](PusherBase *ptr){
        onceToken token(nullptr,[&](){
            delete  ptr;
        });
        ptr->teardown();
    };
    std::string prefix = FindField(url.data(), NULL, "://");

    if (strcasecmp("rtsps",prefix.data()) == 0) {
        return PusherBase::Ptr(new TcpClientWithSSL<RtspPusherImp>(poller, std::dynamic_pointer_cast<RtspMediaSource>(src)), releasePusher);
    }

    if (strcasecmp("rtsp",prefix.data()) == 0) {
        return PusherBase::Ptr(new RtspPusherImp(poller, std::dynamic_pointer_cast<RtspMediaSource>(src)), releasePusher);
    }

    if (strcasecmp("rtmps",prefix.data()) == 0) {
        return PusherBase::Ptr(new TcpClientWithSSL<RtmpPusherImp>(poller, std::dynamic_pointer_cast<RtmpMediaSource>(src)), releasePusher);
    }

    if (strcasecmp("rtmp",prefix.data()) == 0) {
        return PusherBase::Ptr(new RtmpPusherImp(poller, std::dynamic_pointer_cast<RtmpMediaSource>(src)), releasePusher);
    }

    throw std::invalid_argument("not supported push schema:" + url);
}

PusherBase::PusherBase() {
    this->mINI::operator[](Client::kTimeoutMS) = 10000;
    this->mINI::operator[](Client::kBeatIntervalMS) = 5000;
}

} /* namespace mediakit */
