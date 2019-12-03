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
