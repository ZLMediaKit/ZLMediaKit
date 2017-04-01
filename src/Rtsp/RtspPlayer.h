/*
 * RtspPlayer.h
 *
 *  Created on: 2016年8月17日
 *      Author: xzl
 */

#ifndef SRC_RTSPPLAYER_RTSPPLAYER_H_TXT_
#define SRC_RTSPPLAYER_RTSPPLAYER_H_TXT_
#include <netinet/in.h>
#include "Player/PlayerBase.h"
#include "Poller/Timer.hpp"
#include "RtspMediaSource.h"
#include <string>
#include "RtspSession.h"
#include "Util/logger.h"
#include "Util/util.h"
#include "Network/Socket.hpp"
#include "Network/TcpClient.h"
#include "Util/TimeTicker.h"
#include "Rtsp.h"
#include <memory>

using namespace std;
using namespace ZL::Util;
using namespace ZL::Network;
using namespace ZL::Rtsp;
using namespace ZL::Poller;
using namespace ZL::Player;
namespace ZL {
namespace Rtsp {

//实现了rtsp播放器协议部分的功能
class RtspPlayer: public PlayerBase,public TcpClient {
public:
	typedef std::shared_ptr<RtspPlayer> Ptr;
	RtspPlayer();
	virtual ~RtspPlayer(void);
	void play(const char* strUrl, const char *strUser, const char *strPwd,  eRtpType eType) override;
	void pause(bool bPause) override;
	void teardown() override;
	float getRtpLossRate(int iTrackId) const override;
protected:
	//派生类回调函数
	virtual bool onCheckSDP(const string &strSdp, const RtspTrack *pTrack, int iTrackCnt) = 0;
	virtual void onRecvRTP(const RtpPacket::Ptr &pRtppt, const RtspTrack &track) = 0;
    float getProgressTime() const;
    void seekToTime(float fTime);
private:
	 void _onShutdown(const SockException &ex) {
		WarnL << ex.getErrCode() << " " << ex.what();
		m_pPlayTimer.reset();
		m_pRtpTimer.reset();
		m_pBeatTimer.reset();
		onShutdown(ex);
	}
	void _onRecvRTP(const RtpPacket::Ptr &pRtppt, const RtspTrack &track) {
		m_rtpTicker.resetTime();
		onRecvRTP(pRtppt,track);
	}
	void _onPlayResult(const SockException &ex) {
		WarnL << ex.getErrCode() << " " << ex.what();
        m_pPlayTimer.reset();
        m_pRtpTimer.reset();
		if (!ex) {
			m_rtpTicker.resetTime();
			weak_ptr<RtspPlayer> weakSelf = dynamic_pointer_cast<RtspPlayer>(shared_from_this());
			m_pRtpTimer.reset( new Timer(5, [weakSelf]() {
				auto strongSelf=weakSelf.lock();
				if(!strongSelf) {
					return false;
				}
				if(strongSelf->m_rtpTicker.elapsedTime()>10000) {
					//recv rtp timeout!
					strongSelf->_onShutdown(SockException(Err_timeout,"recv rtp timeout"));
					strongSelf->teardown();
					return false;
				}
				return true;
			}));
		}
		onPlayResult(ex);
	}

	void onConnect(const SockException &err) override;
	void onRecv(const Socket::Buffer::Ptr &pBuf) override;
	void onErr(const SockException &ex) override;

	void HandleResSETUP(const Parser &parser, unsigned int uiTrackIndex);
	void HandleResDESCRIBE(const Parser &parser);
	void HandleResPAUSE(const Parser &parser, bool bPause);

	//发数据给服务器
	inline int write(const char *strMsg, ...);
	inline int onProcess(const char* strBuf);
	//生成rtp包结构体
	inline void splitRtp(unsigned char *pucData, unsigned int uiLen);
	//发送SETUP命令
	inline void sendSetup(unsigned int uiTrackIndex);
    inline void sendPause(bool bPause,float fTime);
	//处理一个rtp包
	inline bool HandleOneRtp(int iTrackidx, unsigned char *ucData, unsigned int uiLen);
	bool sendOptions();
	inline void _onRecvRTP(const RtpPacket::Ptr &pRtppt, int iTrackidx);
	inline int getTrackIndex(int iTrackId) const{
		for (unsigned int i = 0; i < m_uiTrackCnt; i++) {
			if (m_aTrackInfo[i].trackId == iTrackId) {
				return i;
			}
		}
		return -1;
	}

	string m_strUrl;
	unsigned int m_uiTrackCnt = 0;
	RtspTrack m_aTrackInfo[2];

	function<void(const Parser&)> m_onHandshake;
	RtspMediaSource::PoolType m_pktPool;

	uint8_t *m_pucRtpBuf = nullptr;
	unsigned int m_uiRtpBufLen = 0;
	Socket::Ptr m_apUdpSock[2];
	//rtsp info
	string m_strSession;
	unsigned int m_uiCseq = 1;
	uint32_t m_aui32SsrcErrorCnt[2] = { 0, 0 };
	string m_strAuthorization;
	string m_strContentBase;
	eRtpType m_eType = RTP_TCP;
	/* RTP包排序所用参数 */
	uint16_t m_aui16LastSeq[2] = { 0 , 0 };
	uint64_t m_aui64SeqOkCnt[2] = { 0 , 0};
	bool m_abSortStarted[2] = { 0 , 0};
	map<uint32_t , RtpPacket::Ptr> m_amapRtpSort[2];

	/* 丢包率统计需要用到的参数 */
	uint16_t m_aui16FirstSeq[2] = { 0 , 0};
	uint16_t m_aui16NowSeq[2] = { 0 , 0 };
	uint64_t m_aui64RtpRecv[2] = { 0 , 0};

	//超时功能实现
	Ticker m_rtpTicker;
	std::shared_ptr<Timer> m_pPlayTimer;
	std::shared_ptr<Timer> m_pRtpTimer;
	//心跳定时器
	std::shared_ptr<Timer> m_pBeatTimer;
    
    //播放进度控制
    float m_fSeekTo = 0;
    double m_adFistStamp[2] = {0,0};
    double m_adNowStamp[2] = {0,0};
    Ticker m_aNowStampTicker[2];
};

} /* namespace Rtsp */
} /* namespace ZL */

#endif /* SRC_RTSPPLAYER_RTSPPLAYER_H_TXT_ */
