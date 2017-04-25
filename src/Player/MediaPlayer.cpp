/*
 * MediaPlayer.cpp
 *
 *  Created on: 2016年12月5日
 *      Author: xzl
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
	if(!EventPoller::Instance().isMainThread()){
		FatalL << "未在主线程释放";
	}
	teardown();
}

void MediaPlayer::play(const char* strUrl, const char* strUser, const char* strPwd, eRtpType eType) {
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
	m_parser->play(strUrl, strUser, strPwd, eType);
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
