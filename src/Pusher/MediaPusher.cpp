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
#include "MediaPusher.h"
#include "PusherBase.h"

using namespace toolkit;

namespace mediakit {

MediaPusher::MediaPusher(const MediaSource::Ptr &src,
                         const EventPoller::Ptr &poller) {
    _src = src;
    _poller = poller;
    if(!_poller){
        _poller = EventPollerPool::Instance().getPoller();
    }
}

MediaPusher::MediaPusher(const string &schema,
                         const string &strVhost,
                         const string &strApp,
                         const string &strStream,
                         const EventPoller::Ptr &poller) :
        MediaPusher(MediaSource::find(schema,strVhost,strApp,strStream),poller){
}

MediaPusher::~MediaPusher() {
}
void MediaPusher::publish(const string &strUrl) {
    _delegate = PusherBase::createPusher(_poller,_src.lock(),strUrl);
    _delegate->setOnShutdown(_shutdownCB);
    _delegate->setOnPublished(_publishCB);
    _delegate->mINI::operator=(*this);
    _delegate->publish(strUrl);
}

EventPoller::Ptr MediaPusher::getPoller(){
    return _poller;
}



} /* namespace mediakit */
