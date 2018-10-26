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
using namespace toolkit;

namespace mediakit {

//实现了rtsp播放器协议部分的功能
class RtspPlayer: public PlayerBase,public TcpClient {
public:
	typedef std::shared_ptr<RtspPlayer> Ptr;

	RtspPlayer();
	virtual ~RtspPlayer(void);
	void play(const char* strUrl) override;
	void pause(bool bPause) override;
	void teardown() override;
	float getRtpLossRate(int iTrackType) const override;
protected:
	//派生类回调函数
	virtual bool onCheckSDP(const string &strSdp, const SdpAttr &sdpAttr) = 0;
	virtual void onRecvRTP(const RtpPacket::Ptr &pRtppt, const SdpTrack::Ptr &track) = 0;
    float getProgressTime() const;
    void seekToTime(float fTime);
private:
	void onShutdown_l(const SockException &ex);
    void onRecvRTP_l(const RtpPacket::Ptr &pRtppt, int iTrackidx);
	void onRecvRTP_l(const RtpPacket::Ptr &pRtppt, const SdpTrack::Ptr &track);
	void onPlayResult_l(const SockException &ex);

    int getTrackIndexByControlSuffix(const string &controlSuffix) const;
    int getTrackIndexByInterleaved(int interleaved) const;
	int getTrackIndexByTrackType(TrackType trackId) const;

	void play(const char* strUrl, const char *strUser, const char *strPwd,  eRtpType eType);
	void onConnect(const SockException &err) override;
	void onRecv(const Buffer::Ptr &pBuf) override;
	void onErr(const SockException &ex) override;

	void handleResSETUP(const Parser &parser, unsigned int uiTrackIndex);
	void handleResDESCRIBE(const Parser &parser);
	bool handleAuthenticationFailure(const string &wwwAuthenticateParamsStr);
	void handleResPAUSE(const Parser &parser, bool bPause);

	//发数据给服务器
	int onProcess(const char* strBuf);
	//生成rtp包结构体
	void splitRtp(unsigned char *pucData, unsigned int uiLen);
    //处理一个rtp包
    bool handleOneRtp(int iTrackidx, unsigned char *ucData, unsigned int uiLen);

	//发送SETUP命令
    bool sendSetup(unsigned int uiTrackIndex);
    bool sendPause(bool bPause,float fTime);
	bool sendOptions();
	bool sendDescribe();
    bool sendRtspRequest(const string &cmd, const string &url ,const StrCaseMap &header = StrCaseMap());
private:
	string _strUrl;

	SdpAttr _sdpAttr;
	vector<SdpTrack::Ptr> _aTrackInfo;

	function<void(const Parser&)> _onHandshake;
	RtspMediaSource::PoolType _pktPool;

	uint8_t *_pucRtpBuf = nullptr;
	unsigned int _uiRtpBufLen = 0;
	Socket::Ptr _apUdpSock[2];
	//rtsp info
	string _strSession;
	unsigned int _uiCseq = 1;
	uint32_t _aui32SsrcErrorCnt[2] = { 0, 0 };
	string _strContentBase;
	eRtpType _eType = RTP_TCP;
	/* RTP包排序所用参数 */
	uint16_t _aui16LastSeq[2] = { 0 , 0 };
	uint64_t _aui64SeqOkCnt[2] = { 0 , 0};
	bool _abSortStarted[2] = { 0 , 0};
	map<uint32_t , RtpPacket::Ptr> _amapRtpSort[2];

	/* 丢包率统计需要用到的参数 */
	uint16_t _aui16FirstSeq[2] = { 0 , 0};
	uint16_t _aui16NowSeq[2] = { 0 , 0 };
	uint64_t _aui64RtpRecv[2] = { 0 , 0};

	//超时功能实现
	Ticker _rtpTicker;
	std::shared_ptr<Timer> _pPlayTimer;
	std::shared_ptr<Timer> _pRtpTimer;
	//心跳定时器
	std::shared_ptr<Timer> _pBeatTimer;
    
    //播放进度控制
    float _fSeekTo = 0;
    double _adFistStamp[2] = {0,0};
    double _adNowStamp[2] = {0,0};
    Ticker _aNowStampTicker[2];
};

} /* namespace mediakit */

#endif /* SRC_RTSPPLAYER_RTSPPLAYER_H_TXT_ */
