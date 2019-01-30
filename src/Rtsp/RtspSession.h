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
#include "Util/util.h"
#include "Util/logger.h"
#include "Common/config.h"
#include "Network/TcpSession.h"
#include "Player/PlayerBase.h"
#include "Rtsp.h"
#include "RtpBroadCaster.h"
#include "RtspMediaSource.h"
#include "RtspSplitter.h"
#include "RtpReceiver.h"
#include "RtspToRtmpMediaSource.h"

using namespace std;
using namespace toolkit;

namespace mediakit {

class RtspSession;

class BufferRtp : public Buffer{
public:
    typedef std::shared_ptr<BufferRtp> Ptr;
    BufferRtp(const RtpPacket::Ptr & pkt,uint32_t offset = 0 ):_rtp(pkt),_offset(offset){}
    virtual ~BufferRtp(){}

    char *data() const override {
        return (char *)_rtp->payload + _offset;
    }
    uint32_t size() const override {
        return _rtp->length - _offset;
    }
private:
    RtpPacket::Ptr _rtp;
    uint32_t _offset;
};

class RtspSession: public TcpSession, public RtspSplitter, public RtpReceiver , public MediaSourceEvent{
public:
	typedef std::shared_ptr<RtspSession> Ptr;
	typedef std::function<void(const string &realm)> onGetRealm;
    //encrypted为true是则表明是md5加密的密码，否则是明文密码
    //在请求明文密码时如果提供md5密码者则会导致认证失败
	typedef std::function<void(bool encrypted,const string &pwd_or_md5)> onAuth;

	RtspSession(const Socket::Ptr &pSock);
	virtual ~RtspSession();
	void onRecv(const Buffer::Ptr &pBuf) override;
	void onError(const SockException &err) override;
	void onManager() override;
protected:
	//RtspSplitter override
    /**
     * 收到完整的rtsp包回调，包括sdp等content数据
     * @param parser rtsp包
     */
    void onWholeRtspPacket(Parser &parser) override;

    /**
     * 收到rtp包回调
     * @param data
     * @param len
     */
    void onRtpPacket(const char *data,uint64_t len) override;

    /**
     * 从rtsp头中获取Content长度
     * @param parser
     * @return
     */
    int64_t getContentLength(Parser &parser) override;

	//RtpReceiver override
	void onRtpSorted(const RtpPacket::Ptr &rtppt, int trackidx) override;
	//MediaSourceEvent override
	bool close() override ;

	//TcpSession override
    int send(const Buffer::Ptr &pkt) override;
private:
	bool handleReq_Options(const Parser &parser); //处理options方法
    bool handleReq_Describe(const Parser &parser); //处理describe方法
    bool handleReq_ANNOUNCE(const Parser &parser); //处理options方法
    bool handleReq_RECORD(const Parser &parser); //处理options方法
    bool handleReq_Setup(const Parser &parser); //处理setup方法
    bool handleReq_Play(const Parser &parser); //处理play方法
    bool handleReq_Pause(const Parser &parser); //处理pause方法
    bool handleReq_Teardown(const Parser &parser); //处理teardown方法
    bool handleReq_Get(const Parser &parser); //处理Get方法
    bool handleReq_Post(const Parser &parser); //处理Post方法
    bool handleReq_SET_PARAMETER(const Parser &parser); //处理SET_PARAMETER方法

	void inline send_StreamNotFound(); //rtsp资源未找到
	void inline send_UnsupportedTransport(); //不支持的传输模式
	void inline send_SessionNotFound(); //会话id错误
	void inline send_NotAcceptable(); //rtsp同时播放数限制
	inline bool findStream(); //根据rtsp url查找 MediaSource实例
    inline void findStream(const function<void(bool)> &cb); //根据rtsp url查找 MediaSource实例

	inline string printSSRC(uint32_t ui32Ssrc);
	inline int getTrackIndexByTrackType(TrackType type);
    inline int getTrackIndexByControlSuffix(const string &controlSuffix);
	inline int getTrackIndexByInterleaved(int interleaved);

	inline void onRcvPeerUdpData(int iTrackIdx, const Buffer::Ptr &pBuf, const struct sockaddr &addr);
	inline void startListenPeerUdpData(int iTrackIdx);

    //认证相关
    static void onAuthSuccess(const weak_ptr<RtspSession> &weakSelf);
    static void onAuthFailed(const weak_ptr<RtspSession> &weakSelf,const string &realm);
    static void onAuthUser(const weak_ptr<RtspSession> &weakSelf,const string &realm,const string &authorization);
    static void onAuthBasic(const weak_ptr<RtspSession> &weakSelf,const string &realm,const string &strBase64);
    static void onAuthDigest(const weak_ptr<RtspSession> &weakSelf,const string &realm,const string &strMd5);

    void doDelay(int delaySec,const std::function<void()> &fun);
    void cancelDelyaTask();

	inline void sendRtpPacket(const RtpPacket::Ptr &pkt);
	bool sendRtspResponse(const string &res_code,const std::initializer_list<string> &header, const string &sdp = "" , const char *protocol = "RTSP/1.0");
	bool sendRtspResponse(const string &res_code,const StrCaseMap &header = StrCaseMap(), const string &sdp = "",const char *protocol = "RTSP/1.0");
private:
	Ticker _ticker;
	int _iCseq = 0;
	string _strContentBase;
	string _strSdp;
	string _strSession;
	bool _bFirstPlay = true;
    MediaInfo _mediaInfo;
	std::weak_ptr<RtspMediaSource> _pMediaSrc;
	RingBuffer<RtpPacket::Ptr>::RingReader::Ptr _pRtpReader;
	PlayerBase::eRtpType _rtpType = PlayerBase::RTP_Invalid;
	vector<SdpTrack::Ptr> _aTrackInfo;

	//RTP over udp
	bool _bGotAllPeerUdp = false;
	bool _abGotPeerUdp[2] = { false, false }; //获取客户端udp端口计数
	Socket::Ptr _apRtpSock[2]; //RTP端口,trackid idx 为数组下标
	Socket::Ptr _apRtcpSock[2];//RTCP端口,trackid idx 为数组下标
	std::shared_ptr<struct sockaddr> _apPeerRtpPortAddr[2]; //播放器接收RTP的地址,trackid idx 为数组下标

	//RTP over udp_multicast
	RtpBroadCaster::Ptr _pBrdcaster;

	//登录认证
    string _strNonce;
    //消耗的总流量
    uint64_t _ui64TotalBytes = 0;

	//RTSP over HTTP
	//quicktime 请求rtsp会产生两次tcp连接，
	//一次发送 get 一次发送post，需要通过x-sessioncookie关联起来
	string _http_x_sessioncookie;
	function<void(const Buffer::Ptr &pBuf)> _onRecv;

    std::function<void()> _delayTask;
    uint32_t _iTaskTimeLine = 0;
    atomic<bool> _enableSendRtp;

    //rtsp推流相关
	RtspToRtmpMediaSource::Ptr _pushSrc;

#ifdef RTSP_SEND_RTCP
	RtcpCounter _aRtcpCnt[2]; //rtcp统计,trackid idx 为数组下标
	Ticker _aRtcpTicker[2]; //rtcp发送时间,trackid idx 为数组下标
	inline void sendRTCP();
#endif
};

/**
 * 支持ssl加密的rtsp服务器，可用于诸如亚马逊echo show这样的设备访问
 */
typedef TcpSessionWithSSL<RtspSession> RtspSessionWithSSL;

} /* namespace mediakit */

#endif /* SESSION_RTSPSESSION_H_ */
