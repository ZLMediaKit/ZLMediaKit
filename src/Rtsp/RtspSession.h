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

#ifndef SESSION_RTSPSESSION_H_
#define SESSION_RTSPSESSION_H_

#include <set>
#include <vector>
#include <unordered_map>
#include "Common/config.h"
#include "Rtsp.h"
#include "RtpBroadCaster.h"
#include "RtspMediaSource.h"
#include "Player/PlayerBase.h"
#include "Util/util.h"
#include "Util/logger.h"
#include "Network/TcpSession.h"

using namespace std;
using namespace ZL::Util;
using namespace ZL::Rtsp;
using namespace ZL::Player;
using namespace ZL::Network;

namespace ZL {
namespace Rtsp {

class RtspSession;

class BufferRtp : public Buffer{
public:
    typedef std::shared_ptr<BufferRtp> Ptr;
    BufferRtp(const RtpPacket::Ptr & pkt,uint32_t offset = 0 ):_rtp(pkt),_offset(offset){}
    virtual ~BufferRtp(){}

    char *data() override {
        return (char *)_rtp->payload + _offset;
    }
    uint32_t size() const override {
        return _rtp->length - _offset;
    }
private:
    RtpPacket::Ptr _rtp;
    uint32_t _offset;
};

class RtspSession: public TcpSession {
public:
	typedef std::shared_ptr<RtspSession> Ptr;
	typedef std::function<void(const string &realm)> onGetRealm;
    //encrypted为true是则表明是md5加密的密码，否则是明文密码
    //在请求明文密码时如果提供md5密码者则会导致认证失败
	typedef std::function<void(bool encrypted,const string &pwd_or_md5)> onAuth;

	RtspSession(const std::shared_ptr<ThreadPool> &pTh, const Socket::Ptr &pSock);
	virtual ~RtspSession();
	void onRecv(const Buffer::Ptr &pBuf) override;
	void onError(const SockException &err) override;
	void onManager() override;
private:
	typedef bool (RtspSession::*rtspCMDHandle)();
	int send(const string &strBuf) override {
        m_ui64TotalBytes += strBuf.size();
		return m_pSender->send(strBuf);
	}
	int send(string &&strBuf) override {
        m_ui64TotalBytes += strBuf.size();
        return m_pSender->send(std::move(strBuf));
	}
	int send(const char *pcBuf, int iSize) override {
        m_ui64TotalBytes += iSize;
        return m_pSender->send(pcBuf, iSize);
	}
	int send(const Buffer::Ptr &pkt) override{
        m_ui64TotalBytes += pkt->size();
        return m_pSender->send(pkt,SOCKET_DEFAULE_FLAGS | FLAG_MORE);
	}
	void shutdown() override;
	bool handleReq_Options(); //处理options方法
	bool handleReq_Describe(); //处理describe方法
	bool handleReq_Setup(); //处理setup方法
	bool handleReq_Play(); //处理play方法
	bool handleReq_Pause(); //处理pause方法
	bool handleReq_Teardown(); //处理teardown方法
	bool handleReq_Get(); //处理Get方法
	bool handleReq_Post(); //处理Post方法
	bool handleReq_SET_PARAMETER(); //处理SET_PARAMETER方法

	void inline send_StreamNotFound(); //rtsp资源未找到
	void inline send_UnsupportedTransport(); //不支持的传输模式
	void inline send_SessionNotFound(); //会话id错误
	void inline send_NotAcceptable(); //rtsp同时播放数限制
	inline bool findStream(); //根据rtsp url查找 MediaSource实例

	inline void initSender(const std::shared_ptr<RtspSession> &pSession); //处理rtsp over http，quicktime使用的
	inline void sendRtpPacket(const RtpPacket::Ptr &pkt);
	inline string printSSRC(uint32_t ui32Ssrc) {
		char tmp[9] = { 0 };
		ui32Ssrc = htonl(ui32Ssrc);
		uint8_t *pSsrc = (uint8_t *) &ui32Ssrc;
		for (int i = 0; i < 4; i++) {
			sprintf(tmp + 2 * i, "%02X", pSsrc[i]);
		}
		return tmp;
	}
	inline int getTrackIndexByTrackId(int iTrackId) {
		for (unsigned int i = 0; i < m_uiTrackCnt; i++) {
			if (iTrackId == m_aTrackInfo[i].trackId) {
				return i;
			}
		}
		return -1;
	}
    inline int getTrackIndexByControlSuffix(const string &controlSuffix) {
        for (unsigned int i = 0; i < m_uiTrackCnt; i++) {
            if (controlSuffix == m_aTrackInfo[i].controlSuffix) {
                return i;
            }
        }
        return -1;
    }

	inline void onRcvPeerUdpData(int iTrackIdx, const Buffer::Ptr &pBuf, const struct sockaddr &addr);
	inline void startListenPeerUdpData();

    //认证相关
    static void onAuthSuccess(const weak_ptr<RtspSession> &weakSelf);
    static void onAuthFailed(const weak_ptr<RtspSession> &weakSelf,const string &realm);
    static void onAuthUser(const weak_ptr<RtspSession> &weakSelf,const string &realm,const string &authorization);
    static void onAuthBasic(const weak_ptr<RtspSession> &weakSelf,const string &realm,const string &strBase64);
    static void onAuthDigest(const weak_ptr<RtspSession> &weakSelf,const string &realm,const string &strMd5);

	char *m_pcBuf = nullptr;
	Ticker m_ticker;
	Parser m_parser; //rtsp解析类
	string m_strUrl;
	string m_strSdp;
	string m_strSession;
	bool m_bFirstPlay = true;
    MediaInfo m_mediaInfo;
	std::weak_ptr<RtspMediaSource> m_pMediaSrc;
	static unordered_map<string, rtspCMDHandle> g_mapCmd;

	//RTP缓冲
	weak_ptr<RingBuffer<RtpPacket::Ptr> > m_pWeakRing;
	RingBuffer<RtpPacket::Ptr>::RingReader::Ptr m_pRtpReader;

	PlayerBase::eRtpType m_rtpType = PlayerBase::RTP_UDP;
	bool m_bSetUped = false;
	int m_iCseq = 0;
	unsigned int m_uiTrackCnt = 0; //媒体track个数
	RtspTrack m_aTrackInfo[2]; //媒体track信息,trackid idx 为数组下标
	bool m_bGotAllPeerUdp = false;

#ifdef RTSP_SEND_RTCP
	RtcpCounter m_aRtcpCnt[2]; //rtcp统计,trackid idx 为数组下标
	Ticker m_aRtcpTicker[2]; //rtcp发送时间,trackid idx 为数组下标
	inline void sendRTCP();
#endif

	//RTP over UDP
	bool m_abGotPeerUdp[2] = { false, false }; //获取客户端udp端口计数
	weak_ptr<Socket> m_apUdpSock[2]; //发送RTP的UDP端口,trackid idx 为数组下标
	std::shared_ptr<struct sockaddr> m_apPeerUdpAddr[2]; //播放器接收RTP的地址,trackid idx 为数组下标
	bool m_bListenPeerUdpData = false;
	RtpBroadCaster::Ptr m_pBrdcaster;

	//登录认证
    string m_strNonce;

	//RTSP over HTTP
	function<void(void)> m_onDestory;
	bool m_bBase64need = false; //是否需要base64解码
	Socket::Ptr m_pSender; //回复rtsp时走的tcp通道，供quicktime用
	//quicktime 请求rtsp会产生两次tcp连接，
	//一次发送 get 一次发送post，需要通过sessioncookie关联起来
	string m_strSessionCookie;

    //消耗的总流量
    uint64_t m_ui64TotalBytes = 0;

	static recursive_mutex g_mtxGetter; //对quicktime上锁保护
	static recursive_mutex g_mtxPostter; //对quicktime上锁保护
	static unordered_map<string, weak_ptr<RtspSession> > g_mapGetter;
	static unordered_map<void *, std::shared_ptr<RtspSession> > g_mapPostter;

};

} /* namespace Session */
} /* namespace ZL */

#endif /* SESSION_RTSPSESSION_H_ */
