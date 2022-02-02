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
#include "MediaPlayer.h"
#include "Rtmp/RtmpPlayerImp.h"
#include "Rtsp/RtspPlayerImp.h"

using namespace std;
using namespace toolkit;

namespace mediakit {

MediaPlayer::MediaPlayer(const EventPoller::Ptr &poller) {
    _poller = poller ? poller : EventPollerPool::Instance().getPoller();
}

static void setOnCreateSocket_l(const std::shared_ptr<PlayerBase> &delegate, const Socket::onCreateSocket &cb){
    auto helper = dynamic_pointer_cast<SocketHelper>(delegate);
    if (helper) {
        if (cb) {
            helper->setOnCreateSocket(cb);
        } else {
            //客户端，确保开启互斥锁
            helper->setOnCreateSocket([](const EventPoller::Ptr &poller) {
                return Socket::createSocket(poller, true);
            });
        }
    }
}

void MediaPlayer::play(const string &url) {
    _delegate = PlayerBase::createPlayer(_poller, url);
    assert(_delegate);
    setOnCreateSocket_l(_delegate, _on_create_socket);
    _delegate->setOnShutdown(_on_shutdown);
    _delegate->setOnPlayResult(_on_play_result);
    _delegate->setOnResume(_on_resume);
    _delegate->setMediaSource(_media_src);
    _delegate->mINI::operator=(*this);
    _delegate->play(url);
}

EventPoller::Ptr MediaPlayer::getPoller(){
    return _poller;
}

void MediaPlayer::setOnCreateSocket(Socket::onCreateSocket cb){
    setOnCreateSocket_l(_delegate, cb);
    _on_create_socket = std::move(cb);
}

} /* namespace mediakit */
