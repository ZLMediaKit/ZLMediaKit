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

#ifndef SRC_RTSPPLAYER_RTSPPLAYER_H_TXT_
#define SRC_RTSPPLAYER_RTSPPLAYER_H_TXT_

#include <string>
#include <memory>
#include "Rtsp.h"
#include "RtspSession.h"
#include "RtspMediaSource.h"
#include "Player/PlayerBase.h"
#include "Util/util.h"
#include "Util/logger.h"
#include "Util/TimeTicker.h"
#include "Poller/Timer.h"
#include "Network/Socket.h"
#include "Network/TcpClient.h"

using namespace std;
using namespace ZL::Rtsp;
using namespace ZL::Player;
using namespace ZL::Util;
using namespace ZL::Poller;
using namespace ZL::Network;

namespace ZL {
namespace Rtsp {

//实现了rtsp播放器协议部分的功能
class RtspPlayer: public PlayerBase,public TcpClient {
public:
	typedef std::shared_ptr<RtspPlayer> Ptr;
	//设置rtp传输类型，可选项有0(tcp，默认)、1(udp)、2(组播)
	//设置方法:player[RtspPlayer::kRtpType] = 0/1/2;
	static const char kRtpType[];

	RtspPlayer();
	virtual ~RtspPlayer(void);
	void play(const char* strUrl) override;
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

	void play(const char* strUrl, const char *strUser, const char *strPwd,  eRtpType eType);
	void onConnect(const SockException &err) override;
	void onRecv(const Buffer::Ptr &pBuf) override;
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
	inline int getTrackIndex(const string &controlSuffix) const{
		for (unsigned int i = 0; i < m_uiTrackCnt; i++) {
			if (m_aTrackInfo[i].controlSuffix == controlSuffix) {
				return i;
			}
		}
		return -1;
	}
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
