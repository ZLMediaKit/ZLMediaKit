/*
* MIT License
*
* Copyright (c) 2016 xiongziliang <771730766@qq.com>
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
#include "Rtsp/Rtsp.h"
#include "Rtsp/RtspPusher.h"
#include "Rtmp/RtmpPusher.h"

using namespace toolkit;

namespace mediakit {

const char PusherBase::kNetAdapter[] = "net_adapter";
const char PusherBase::kRtpType[] = "rtp_type";
const char PusherBase::kRtspUser[] = "rtsp_user" ;
const char PusherBase::kRtspPwd[] = "rtsp_pwd";
const char PusherBase::kRtspPwdIsMD5[] = "rtsp_pwd_md5";

const char PusherBase::kPlayTimeoutMS[] = "play_timeout_ms";
const char PusherBase::kMediaTimeoutMS[] = "media_timeout_ms";
const char PusherBase::kBeatIntervalMS[] = "beat_interval_ms";


PusherBase::Ptr PusherBase::createPusher(const MediaSource::Ptr &src,
                                         const string & strUrl) {
    static auto releasePusher = [](PusherBase *ptr){
        onceToken token(nullptr,[&](){
            delete  ptr;
        });
        ptr->teardown();
    };
    string prefix = FindField(strUrl.data(), NULL, "://");
    if (strcasecmp("rtsp",prefix.data()) == 0) {
        return PusherBase::Ptr(new RtspPusher(dynamic_pointer_cast<RtspMediaSource>(src)),releasePusher);
    }
    if (strcasecmp("rtmp",prefix.data()) == 0) {
        return PusherBase::Ptr(new RtmpPusher(dynamic_pointer_cast<RtmpMediaSource>(src)),releasePusher);
    }
    return PusherBase::Ptr(new RtspPusher(dynamic_pointer_cast<RtspMediaSource>(src)),releasePusher);
}

PusherBase::PusherBase() {
    this->mINI::operator[](kPlayTimeoutMS) = 10000;
    this->mINI::operator[](kMediaTimeoutMS) = 5000;
    this->mINI::operator[](kBeatIntervalMS) = 5000;
}

} /* namespace mediakit */
