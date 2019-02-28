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
using namespace toolkit;

namespace mediakit {

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
	uint32_t getProgressMilliSecond() const;
	void seekToMilliSecond(uint32_t ms);
protected:
	void _onShutdown(const SockException &ex) {
		WarnL << ex.getErrCode() << " " << ex.what();
		_pPlayTimer.reset();
		_pMediaTimer.reset();
		_pBeatTimer.reset();
		onShutdown(ex);
	}
	void _onMediaData(const RtmpPacket::Ptr &chunkData) {
		_mediaTicker.resetTime();
		onMediaData(chunkData);
	}
	void _onPlayResult(const SockException &ex) {
		WarnL << ex.getErrCode() << " " << ex.what();
		_pPlayTimer.reset();
		_pMediaTimer.reset();
		if (!ex) {
			_mediaTicker.resetTime();
			weak_ptr<RtmpPlayer> weakSelf = dynamic_pointer_cast<RtmpPlayer>(shared_from_this());
			_pMediaTimer.reset( new Timer(5, [weakSelf]() {
				auto strongSelf=weakSelf.lock();
				if(!strongSelf) {
					return false;
				}
				if(strongSelf->_mediaTicker.elapsedTime()>10000) {
					//recv media timeout!
					strongSelf->_onShutdown(SockException(Err_timeout,"recv rtmp timeout"));
					strongSelf->teardown();
					return false;
				}
				return true;
			},getPoller()));
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
    void onSendRawData(const Buffer::Ptr &buffer) override{
        send(buffer);
    }

	template<typename FUN>
	inline void addOnResultCB(const FUN &fun) {
		lock_guard<recursive_mutex> lck(_mtxOnResultCB);
		_mapOnResultCB.emplace(_iReqID, fun);
	}
	template<typename FUN>
	inline void addOnStatusCB(const FUN &fun) {
		lock_guard<recursive_mutex> lck(_mtxOnStatusCB);
		_dqOnStatusCB.emplace_back(fun);
	}

	void onCmd_result(AMFDecoder &dec);
	void onCmd_onStatus(AMFDecoder &dec);
	void onCmd_onMetaData(AMFDecoder &dec);

	inline void send_connect();
	inline void send_createStream();
	inline void send_play();
	inline void send_pause(bool bPause);

	string _strApp;
	string _strStream;
	string _strTcUrl;
	bool _bPaused = false;

	unordered_map<int, function<void(AMFDecoder &dec)> > _mapOnResultCB;
	recursive_mutex _mtxOnResultCB;
	deque<function<void(AMFValue &dec)> > _dqOnStatusCB;
	recursive_mutex _mtxOnStatusCB;

	typedef void (RtmpPlayer::*rtmpCMDHandle)(AMFDecoder &dec);
	static unordered_map<string, rtmpCMDHandle> g_mapCmd;

	//超时功能实现
	Ticker _mediaTicker;
	std::shared_ptr<Timer> _pMediaTimer;
	std::shared_ptr<Timer> _pPlayTimer;
	//心跳定时器
	std::shared_ptr<Timer> _pBeatTimer;

	//播放进度控制
	uint32_t _iSeekTo = 0;
	uint32_t _aiFistStamp[2] = { 0, 0 };
	uint32_t _aiNowStamp[2] = { 0, 0 };
	Ticker _aNowStampTicker[2];
};

} /* namespace mediakit */

#endif /* SRC_RTMP_RtmpPlayer2_H_ */
