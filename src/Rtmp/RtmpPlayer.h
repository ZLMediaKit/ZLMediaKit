/*
 * RtmpPlayer2.h
 *
 *  Created on: 2016年11月29日
 *      Author: xzl
 */

#ifndef SRC_RTMP_RtmpPlayer2_H_
#define SRC_RTMP_RtmpPlayer2_H_

#include <netinet/in.h>
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

	void play(const char* strUrl, const char *strUser, const char *strPwd,
			eRtpType eType) override;
	void pause(bool bPause) override;
	void teardown() override;
protected:
	virtual bool onCheckMeta(AMFValue &val) =0;
	virtual void onMediaData(RtmpPacket &chunkData) =0;
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
	void _onMediaData(RtmpPacket &chunkData) {
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
	void onRecv(const Socket::Buffer::Ptr &pBuf) override;
	void onConnect(const SockException &err) override;
	void onErr(const SockException &ex) override;
	//fro RtmpProtocol
	void onRtmpChunk(RtmpPacket &chunkData) override;
	void onStreamDry(uint32_t ui32StreamId) override;
	void onSendRawData(const char *pcRawData, int iSize) override {
		send(pcRawData, iSize);
	}

	template<typename FUN>
	inline void addOnResultCB(const FUN &fun) {
		m_mapOnResultCB.emplace(m_iReqID, fun);
	}
	template<typename FUN>
	inline void addOnStatusCB(const FUN &fun) {
		lock_guard<recursive_mutex> lck(m_mtxDeque);
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
	deque<function<void(AMFValue &dec)> > m_dqOnStatusCB;
	recursive_mutex m_mtxDeque;

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
