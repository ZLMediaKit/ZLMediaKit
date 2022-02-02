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
#include "MediaPusher.h"
#include "PusherBase.h"

using namespace std;
using namespace toolkit;

namespace mediakit {

MediaPusher::MediaPusher(const MediaSource::Ptr &src,
                         const EventPoller::Ptr &poller) {
    _src = src;
    _poller = poller ? poller : EventPollerPool::Instance().getPoller();
}

MediaPusher::MediaPusher(const string &schema,
                         const string &vhost,
                         const string &app,
                         const string &stream,
                         const EventPoller::Ptr &poller) :
        MediaPusher(MediaSource::find(schema, vhost, app, stream), poller){
}

MediaPusher::~MediaPusher() {
}

static void setOnCreateSocket_l(const std::shared_ptr<PusherBase> &delegate, const Socket::onCreateSocket &cb){
    auto helper = dynamic_pointer_cast<SocketHelper>(delegate);
    if (helper) {
        helper->setOnCreateSocket(cb);
    }
}

void MediaPusher::publish(const string &url) {
    _delegate = PusherBase::createPusher(_poller, _src.lock(), url);
    assert(_delegate);
    setOnCreateSocket_l(_delegate, _on_create_socket);
    _delegate->setOnShutdown(_on_shutdown);
    _delegate->setOnPublished(_on_publish);
    _delegate->mINI::operator=(*this);
    _delegate->publish(url);
}

EventPoller::Ptr MediaPusher::getPoller(){
    return _poller;
}

void MediaPusher::setOnCreateSocket(Socket::onCreateSocket cb){
    setOnCreateSocket_l(_delegate, cb);
    _on_create_socket = std::move(cb);
}

} /* namespace mediakit */
