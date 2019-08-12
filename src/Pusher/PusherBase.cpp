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

#include <algorithm>
#include "PusherBase.h"
#include "Rtsp/RtspPusher.h"
#include "Rtmp/RtmpPusher.h"

using namespace toolkit;
using namespace mediakit::Client;

namespace mediakit {

PusherBase::Ptr PusherBase::createPusher(const EventPoller::Ptr &poller,
                                         const MediaSource::Ptr &src,
                                         const string & strUrl) {
    static auto releasePusher = [](PusherBase *ptr){
        onceToken token(nullptr,[&](){
            delete  ptr;
        });
        ptr->teardown();
    };
    string prefix = FindField(strUrl.data(), NULL, "://");

    if (strcasecmp("rtsps",prefix.data()) == 0) {
        return PusherBase::Ptr(new TcpClientWithSSL<RtspPusher>(poller,dynamic_pointer_cast<RtspMediaSource>(src)),releasePusher);
    }

    if (strcasecmp("rtsp",prefix.data()) == 0) {
        return PusherBase::Ptr(new RtspPusher(poller,dynamic_pointer_cast<RtspMediaSource>(src)),releasePusher);
    }

    if (strcasecmp("rtmps",prefix.data()) == 0) {
        return PusherBase::Ptr(new TcpClientWithSSL<RtmpPusher>(poller,dynamic_pointer_cast<RtmpMediaSource>(src)),releasePusher);
    }

    if (strcasecmp("rtmp",prefix.data()) == 0) {
        return PusherBase::Ptr(new RtmpPusher(poller,dynamic_pointer_cast<RtmpMediaSource>(src)),releasePusher);
    }

    return PusherBase::Ptr(new RtspPusher(poller,dynamic_pointer_cast<RtspMediaSource>(src)),releasePusher);
}

PusherBase::PusherBase() {
    this->mINI::operator[](kTimeoutMS) = 10000;
    this->mINI::operator[](kBeatIntervalMS) = 5000;
}

} /* namespace mediakit */
