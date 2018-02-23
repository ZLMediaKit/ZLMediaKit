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

#ifndef SRC_RTMP_RtmpPlayer2_H_
#define SRC_RTMP_RtmpPlayer2_H_

#include <memory>
#include <string>
#include <functional>
#include "amf.h"
#include "Rtmp.h"
#include "RtmpProtocol.h"
#include "Player/PlayerBase.h"
#include "Util/util.h"
#include "Util/logger.h"
#include "Util/TimeTicker.h"
#include "Network/Socket.h"
#include "Network/TcpClient.h"

using namespace std;
using namespace ZL::Util;
using namespace ZL::Player;
using namespace ZL::Network;

namespace ZL {
namespace Rtmp {

class RtmpPlayer:public PlayerBase, public TcpClient,  public RtmpProtocol{
public:
	typedef std::shared_ptr<RtmpPlayer> Ptr;
	RtmpPlayer();
	virtual ~RtmpPlayer();

	void play(const char* strUrl) override;
	void pause(bool bPause) override;
	void teardown() override;
protected:
	virtual bool onCheckMeta(AMFValue &val) =0;
	virtual void onMediaData(const RtmpPacket::Ptr &chunkData) =0;
	float getProgressTime() const;
	void seekToTime(float fTime);
private:
	void _onShutdown(const SockException &ex) {
		WarnL << ex.getErrCode() << " " << ex.what();
		m_pPlayTimer.reset();
		m_pMediaTimer.reset();
		m_pBeatTimer.reset();
		onShutdown(ex);
	}
	void _onMediaData(const RtmpPacket::Ptr &chunkData) {
		m_mediaTicker.resetTime();
		onMediaData(chunkData);
	}
	void _onPlayResult(const SockException &ex) {
		WarnL << ex.getErrCode() << " " << ex.what();
		m_pPlayTimer.reset();
		m_pMediaTimer.reset();
		if (!ex) {
			m_mediaTicker.resetTime();
			weak_ptr<RtmpPlayer> weakSelf = dynamic_pointer_cast<RtmpPlayer>(shared_from_this());
			m_pMediaTimer.reset( new Timer(5, [weakSelf]() {
				auto strongSelf=weakSelf.lock();
				if(!strongSelf) {
					return false;
				}
				if(strongSelf->m_mediaTicker.elapsedTime()>10000) {
					//recv media timeout!
					strongSelf->_onShutdown(SockException(Err_timeout,"recv rtmp timeout"));
					strongSelf->teardown();
					return false;
				}
				return true;
			}));
		}
		onPlayResult(ex);
	}

	//for Tcpclient
	void onRecv(const Buffer::Ptr &pBuf) override;
	void onConnect(const SockException &err) override;
	void onErr(const SockException &ex) override;
	//fro RtmpProtocol
	void onRtmpChunk(RtmpPacket &chunkData) override;
	void onStreamDry(uint32_t ui32StreamId) override;
	void onSendRawData(const char *pcRawData, int iSize) override {
		send(pcRawData, iSize);
	}
    void onSendRawData(const Buffer::Ptr &buffer,int flags) override{
        _sock->send(buffer,flags);
    }

	template<typename FUN>
	inline void addOnResultCB(const FUN &fun) {
		lock_guard<recursive_mutex> lck(m_mtxOnResultCB);
		m_mapOnResultCB.emplace(m_iReqID, fun);
	}
	template<typename FUN>
	inline void addOnStatusCB(const FUN &fun) {
		lock_guard<recursive_mutex> lck(m_mtxOnStatusCB);
		m_dqOnStatusCB.emplace_back(fun);
	}

	void onCmd_result(AMFDecoder &dec);
	void onCmd_onStatus(AMFDecoder &dec);
	void onCmd_onMetaData(AMFDecoder &dec);

	inline void send_connect();
	inline void send_createStream();
	inline void send_play();
	inline void send_pause(bool bPause);

	string m_strApp;
	string m_strStream;
	string m_strTcUrl;
	bool m_bPaused = false;

	unordered_map<int, function<void(AMFDecoder &dec)> > m_mapOnResultCB;
	recursive_mutex m_mtxOnResultCB;
	deque<function<void(AMFValue &dec)> > m_dqOnStatusCB;
	recursive_mutex m_mtxOnStatusCB;

	typedef void (RtmpPlayer::*rtmpCMDHandle)(AMFDecoder &dec);
	static unordered_map<string, rtmpCMDHandle> g_mapCmd;

	//超时功能实现
	Ticker m_mediaTicker;
	std::shared_ptr<Timer> m_pMediaTimer;
	std::shared_ptr<Timer> m_pPlayTimer;
	//心跳定时器
	std::shared_ptr<Timer> m_pBeatTimer;

	//播放进度控制
	float m_fSeekTo = 0;
	double m_adFistStamp[2] = { 0, 0 };
	double m_adNowStamp[2] = { 0, 0 };
	Ticker m_aNowStampTicker[2];
};

} /* namespace Rtmp */
} /* namespace ZL */

#endif /* SRC_RTMP_RtmpPlayer2_H_ */
