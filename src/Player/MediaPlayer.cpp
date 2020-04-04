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
#include "MediaPlayer.h"
#include "Rtmp/RtmpPlayerImp.h"
#include "Rtsp/RtspPlayerImp.h"
using namespace toolkit;

namespace mediakit {

MediaPlayer::MediaPlayer(const EventPoller::Ptr &poller) {
    _poller = poller;
    if(!_poller){
        _poller = EventPollerPool::Instance().getPoller();
    }
}

MediaPlayer::~MediaPlayer() {
}
void MediaPlayer::play(const string &strUrl) {
    _delegate = PlayerBase::createPlayer(_poller,strUrl);
    _delegate->setOnShutdown(_shutdownCB);
    _delegate->setOnPlayResult(_playResultCB);
    _delegate->setOnResume(_resumeCB);
    _delegate->setMediaSouce(_pMediaSrc);
    _delegate->mINI::operator=(*this);
    _delegate->play(strUrl);
}

EventPoller::Ptr MediaPlayer::getPoller(){
    return _poller;
}

void MediaPlayer::pause(bool bPause) {
    if (_delegate) {
        _delegate->pause(bPause);
    }
}

void MediaPlayer::teardown() {
    if (_delegate) {
        _delegate->teardown();
    }
}


} /* namespace mediakit */
