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

PlayerProxy::PlayerProxy(const char *strVhost,
                         const char *strApp,
                         const char *strSrc,
                         bool bEnableHls,
                         bool bEnableMp4,
                         int iRetryCount){
	m_strVhost = strVhost;
	m_strApp = strApp;
	m_strSrc = strSrc;
    m_bEnableHls = bEnableHls;
    m_bEnableMp4 = bEnableMp4;
    m_iRetryCount = iRetryCount;
}
void PlayerProxy::play(const char* strUrl) {
	weak_ptr<PlayerProxy> weakSelf = shared_from_this();
	setOnVideoCB( [weakSelf](const H264Frame &data ) {
		auto strongSelf = weakSelf.lock();
		if(!strongSelf){
			return;
		}
		if(strongSelf->m_pChn){
			strongSelf->m_pChn->inputH264((char *)data.data.data(), data.data.size(), 0);
		}else{
			strongSelf->initMedia();
		}
	});
	setOnAudioCB( [weakSelf](const AdtsFrame &data ) {
		auto strongSelf = weakSelf.lock();
		if(!strongSelf){
			return;
		}
		if(strongSelf->m_pChn){
			strongSelf->m_pChn->inputAAC((char *)data.data, data.aac_frame_length, 0);
		}else{
			strongSelf->initMedia();
		}
	});

	std::shared_ptr<int> piFailedCnt(new int(0)); //连续播放失败次数
	string strUrlTmp(strUrl);
	setOnPlayResult([weakSelf,strUrlTmp,piFailedCnt](const SockException &err) {
		auto strongSelf = weakSelf.lock();
		if(!strongSelf) {
			return;
		}
		if(!err) {
			// 播放成功
			*piFailedCnt = 0;//连续播放失败次数清0
		}else if(*piFailedCnt < strongSelf->m_iRetryCount || strongSelf->m_iRetryCount < 0) {
			// 播放失败，延时重试播放
			strongSelf->rePlay(strUrlTmp,(*piFailedCnt)++);
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
		if(*piFailedCnt < strongSelf->m_iRetryCount || strongSelf->m_iRetryCount < 0) {
			strongSelf->rePlay(strUrlTmp,(*piFailedCnt)++);
		}
	});
	MediaPlayer::play(strUrl);
}

PlayerProxy::~PlayerProxy() {
	auto iTaskId = reinterpret_cast<uint64_t>(this);
	AsyncTaskThread::Instance().CancelTask(iTaskId);
}
void PlayerProxy::rePlay(const string &strUrl,int iFailedCnt){
	auto iTaskId = reinterpret_cast<uint64_t>(this);
	auto iDelay = MAX(2 * 1000, MIN(iFailedCnt * 3000,60*1000));
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
	m_pChn.reset(new DevChannel(m_strVhost.data(),m_strApp.data(),m_strSrc.data(),getDuration(),m_bEnableHls,m_bEnableMp4));
    m_pChn->setListener(shared_from_this());
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
bool PlayerProxy::shutDown() {
    //通知其停止推流
    weak_ptr<PlayerProxy> weakSlef = dynamic_pointer_cast<PlayerProxy>(shared_from_this());
    ASYNC_TRACE([weakSlef](){
        auto stronSelf = weakSlef.lock();
        if(stronSelf){
            stronSelf->m_pChn.reset();
            stronSelf->teardown();
        }
    });
    return true;
}


} /* namespace Player */
} /* namespace ZL */
