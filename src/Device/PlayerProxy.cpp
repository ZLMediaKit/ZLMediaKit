/*
 * PlyerProxy.cpp
 *
 *  Created on: 2016年12月6日
 *      Author: xzl
 */

#include "PlayerProxy.h"
#include "Thread/AsyncTaskThread.h"
#include "Util/MD5.h"
#include "Util/logger.h"
#include "config.h"
#include "Util/mini.hpp"

using namespace ZL::Util;
using namespace ZL::Thread;

namespace ZL {
namespace DEV {

PlayerProxy::PlayerProxy(const char *strApp,const char *strSrc){
	m_strApp = strApp;
	m_strSrc = strSrc;
}

void PlayerProxy::play(const char* strUrl, const char *strUser,
		const char *strPwd, PlayerBase::eRtpType eType, uint32_t iSecond) {
	m_aliveSecond = iSecond;
	string strUrlTmp(strUrl);
	string strUserTmp(strUser);
	string strPwdTmp(strPwd);

	m_pPlayer.reset(new MediaPlayer());
	m_pPlayer->play(strUrl, strUser, strPwd, eType);
	weak_ptr<PlayerProxy> weakSelf = shared_from_this();
	m_pPlayer->setOnVideoCB( [weakSelf,strUrlTmp](const H264Frame &data ) {
		auto strongSelf = weakSelf.lock();
		if(!strongSelf){
			return;
		}
		if(strongSelf->m_pChn){
			strongSelf->m_pChn->inputH264((char *)data.data.data(), data.data.size(), data.timeStamp);
		}else{
			strongSelf->initMedia();
		}
		strongSelf->checkExpired();
	});
	m_pPlayer->setOnAudioCB( [weakSelf,strUrlTmp](const AdtsFrame &data ) {
		auto strongSelf = weakSelf.lock();
		if(!strongSelf){
			return;
		}
		if(strongSelf->m_pChn){
			strongSelf->m_pChn->inputAAC((char *)data.data, data.aac_frame_length, data.timeStamp);
		}else{
			strongSelf->initMedia();
		}
		strongSelf->checkExpired();
	});

	std::shared_ptr<uint64_t> piFailedCnt(new uint64_t(0)); //连续播放失败次数
	m_pPlayer->setOnPlayResult([weakSelf,strUrlTmp,strUserTmp,strPwdTmp,eType,piFailedCnt](const SockException &err) {
		auto strongSelf = weakSelf.lock();
		if(!strongSelf) {
			return;
		}
		static uint64_t replayCnt = mINI::Instance()[Config::Proxy::kReplayCount].as<uint64_t>();
		if(!err) {
			// 播放成功
			*piFailedCnt = 0;//连续播放失败次数清0
		}else if(*piFailedCnt < replayCnt) {
			// 播放失败，延时重试播放
			strongSelf->rePlay(strUrlTmp, strUserTmp, strPwdTmp, eType,(*piFailedCnt)++);
		}else{
			strongSelf->expired();
		}
	});
	weak_ptr<MediaPlayer> weakPtr= m_pPlayer;
	m_pPlayer->setOnShutdown([weakSelf,weakPtr,strUrlTmp,strUserTmp,strPwdTmp,eType,piFailedCnt](const SockException &err) {
		auto strongSelf = weakSelf.lock();
		if(!strongSelf) {
			return;
		}
		if(strongSelf->m_pChn) {
			strongSelf->m_pChn.reset();
		}
		//播放异常中断，延时重试播放
		static uint64_t replayCnt = mINI::Instance()[Config::Proxy::kReplayCount].as<uint64_t>();
		if(*piFailedCnt < replayCnt) {
			strongSelf->rePlay(strUrlTmp, strUserTmp, strPwdTmp, eType,(*piFailedCnt)++);
		}else{
			strongSelf->expired();
		}
	});
}

PlayerProxy::~PlayerProxy() {
	auto iTaskId = reinterpret_cast<uint64_t>(this);
	AsyncTaskThread::Instance().CancelTask(iTaskId);
}
void PlayerProxy::rePlay(const string &strUrl, const string &strUser, const string &strPwd, PlayerBase::eRtpType eType, uint64_t iFailedCnt){
	checkExpired();
	auto iTaskId = reinterpret_cast<uint64_t>(this);
	auto iDelay = MAX((uint64_t)2 * 1000, MIN(iFailedCnt * 3000,(uint64_t)60*1000));
	weak_ptr<MediaPlayer> weakPtr = m_pPlayer;
	AsyncTaskThread::Instance().CancelTask(iTaskId);
	AsyncTaskThread::Instance().DoTaskDelay(iTaskId, iDelay, [weakPtr,strUrl,strUser,strPwd,eType,iFailedCnt]() {
		//播放失败次数越多，则延时越长
		auto strongPlayer = weakPtr.lock();
		if(!strongPlayer) {
			return false;
		}
		WarnL << "重试播放[" << iFailedCnt << "]:"  << strUrl;
		strongPlayer->play(strUrl.data(), strUser.data(), strPwd.data(), eType);
		return false;
	});
}
void PlayerProxy::initMedia() {
	if (!m_pPlayer->isInited()) {
		return;
	}
	m_pChn.reset(new DevChannel(m_strApp.data(),m_strSrc.data(),m_pPlayer->getDuration()));
	if (m_pPlayer->containVideo()) {
		VideoInfo info;
		info.iFrameRate = m_pPlayer->getVideoFps();
		info.iWidth = m_pPlayer->getVideoWidth();
		info.iHeight = m_pPlayer->getVideoHeight();
		m_pChn->initVideo(info);
	}
	if (m_pPlayer->containAudio()) {
		AudioInfo info;
		info.iSampleRate = m_pPlayer->getAudioSampleRate();
		info.iChannel = m_pPlayer->getAudioChannel();
		info.iSampleBit = m_pPlayer->getAudioSampleBit();
		m_pChn->initAudio(info);
	}
}

void PlayerProxy::checkExpired() {
	if(m_aliveSecond && m_aliveTicker.elapsedTime() > m_aliveSecond * 1000){
		//到期
		expired();
	}
}

void PlayerProxy::expired() {
	if(onExpired){
		onExpired();
	}
}

} /* namespace Player */
} /* namespace ZL */
