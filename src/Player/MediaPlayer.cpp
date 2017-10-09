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
#include "MediaPlayer.h"
#include "Rtmp/RtmpPlayerImp.h"
#include "Rtsp/RtspPlayerImp.h"

using namespace ZL::Rtmp;
using namespace ZL::Rtsp;

namespace ZL {
namespace Player {

MediaPlayer::MediaPlayer() {
}

MediaPlayer::~MediaPlayer() {
	teardown();
}
void MediaPlayer::play(const char* strUrl) {
	string strPrefix = FindField(strUrl, NULL, "://");
	if ((strcasecmp(m_strPrefix.data(),strPrefix.data()) != 0) || strPrefix.empty()) {
		//协议切换
		m_strPrefix = strPrefix;
		m_parser = PlayerBase::createPlayer(strUrl);
		m_parser->setOnShutdown(m_shutdownCB);
		m_parser->setOnVideoCB(m_onGetVideoCB);
		m_parser->setOnAudioCB(m_onGetAudioCB);
	}
	m_parser->setOnPlayResult(m_playResultCB);
	m_parser->mINI::operator=(*this);
	m_parser->play(strUrl);
}


void MediaPlayer::pause(bool bPause) {
	if (m_parser) {
		m_parser->pause(bPause);
	}
}

void MediaPlayer::teardown() {
	if (m_parser) {
		m_parser->teardown();
	}
}


} /* namespace Player */
} /* namespace ZL */
