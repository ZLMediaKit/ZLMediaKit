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

#include "Common/config.h"
#include "PlayerProxy.h"
#include "Util/mini.h"
#include "Util/MD5.h"
#include "Util/logger.h"
#include "Thread/AsyncTaskThread.h"

using namespace ZL::Util;
using namespace ZL::Thread;

namespace ZL {
namespace DEV {

const char PlayerProxy::kAliveSecond[] = "alive_second";

PlayerProxy::PlayerProxy(const char *strVhost,const char *strApp,const char *strSrc){
	m_strVhost = strVhost;
	m_strApp = strApp;
	m_strSrc = strSrc;
}
void PlayerProxy::play(const char* strUrl) {
	m_aliveSecond = (*this)[kAliveSecond];
	weak_ptr<PlayerProxy> weakSelf = shared_from_this();
	setOnVideoCB( [weakSelf](const H264Frame &data ) {
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
	setOnAudioCB( [weakSelf](const AdtsFrame &data ) {
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
	string strUrlTmp(strUrl);
	setOnPlayResult([weakSelf,strUrlTmp,piFailedCnt](const SockException &err) {
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
			strongSelf->rePlay(strUrlTmp,(*piFailedCnt)++);
		}else{
			strongSelf->expired();
		}
	});
	setOnShutdown([weakSelf,strUrlTmp,piFailedCnt](const SockException &err) {
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
			strongSelf->rePlay(strUrlTmp,(*piFailedCnt)++);
		}else{
			strongSelf->expired();
		}
	});
	MediaPlayer::play(strUrl);
}

PlayerProxy::~PlayerProxy() {
	auto iTaskId = reinterpret_cast<uint64_t>(this);
	AsyncTaskThread::Instance().CancelTask(iTaskId);
}
void PlayerProxy::rePlay(const string &strUrl,uint64_t iFailedCnt){
	checkExpired();
	auto iTaskId = reinterpret_cast<uint64_t>(this);
	auto iDelay = MAX((uint64_t)2 * 1000, MIN(iFailedCnt * 3000,(uint64_t)60*1000));
	weak_ptr<PlayerProxy> weakSelf = shared_from_this();
	AsyncTaskThread::Instance().CancelTask(iTaskId);
	AsyncTaskThread::Instance().DoTaskDelay(iTaskId, iDelay, [weakSelf,strUrl,iFailedCnt]() {
		//播放失败次数越多，则延时越长
		auto strongPlayer = weakSelf.lock();
		if(!strongPlayer) {
			return false;
		}
		WarnL << "重试播放[" << iFailedCnt << "]:"  << strUrl;
		strongPlayer->MediaPlayer::play(strUrl.data());
		return false;
	});
}
void PlayerProxy::initMedia() {
	if (!isInited()) {
		return;
	}
	m_pChn.reset(new DevChannel(m_strVhost.data(),m_strApp.data(),m_strSrc.data(),getDuration()));
	if (containVideo()) {
		VideoInfo info;
		info.iFrameRate = getVideoFps();
		info.iWidth = getVideoWidth();
		info.iHeight = getVideoHeight();
		m_pChn->initVideo(info);
	}
	if (containAudio()) {
		AudioInfo info;
		info.iSampleRate = getAudioSampleRate();
		info.iChannel = getAudioChannel();
		info.iSampleBit = getAudioSampleBit();
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
