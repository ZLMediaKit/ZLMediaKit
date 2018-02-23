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

#ifndef SRC_RTMP_RTMPPUSHER_H_
#define SRC_RTMP_RTMPPUSHER_H_

#include "RtmpProtocol.h"
#include "RtmpMediaSource.h"
#include "Network/TcpClient.h"

namespace ZL {
namespace Rtmp {

class RtmpPusher: public RtmpProtocol , public TcpClient{
public:
	typedef std::shared_ptr<RtmpPusher> Ptr;
	typedef std::function<void(const SockException &ex)> Event;
	RtmpPusher(const char *strVhost,const char *strApp,const char *strStream);
	RtmpPusher(const RtmpMediaSource::Ptr  &src);
	virtual ~RtmpPusher();

	void publish(const char* strUrl);
	void teardown();

	void setOnPublished(Event onPublished) {
		m_onPublished = onPublished;
	}

	void setOnShutdown(Event onShutdown) {
		m_onShutdown = onShutdown;
	}

protected:

	//for Tcpclient
	void onRecv(const Buffer::Ptr &pBuf) override;
	void onConnect(const SockException &err) override;
	void onErr(const SockException &ex) override;

	//fro RtmpProtocol
	void onRtmpChunk(RtmpPacket &chunkData) override;
	void onSendRawData(const char *pcRawData, int iSize) override {
		send(pcRawData, iSize);
	}
	void onSendRawData(const Buffer::Ptr &buffer,int flags) override{
		_sock->send(buffer,flags);
	}
private:
    void init(const RtmpMediaSource::Ptr  &src);
	void onShutdown(const SockException &ex) {
		m_pPublishTimer.reset();
		if(m_onShutdown){
			m_onShutdown(ex);
		}
	}
	void onPublishResult(const SockException &ex) {
		m_pPublishTimer.reset();
		if(m_onPublished){
			m_onPublished(ex);
		}
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
	inline void send_publish();
	inline void send_metaData();

	string m_strApp;
	string m_strStream;
	string m_strTcUrl;

	unordered_map<int, function<void(AMFDecoder &dec)> > m_mapOnResultCB;
	recursive_mutex m_mtxOnResultCB;
	deque<function<void(AMFValue &dec)> > m_dqOnStatusCB;
	recursive_mutex m_mtxOnStatusCB;

	typedef void (RtmpPusher::*rtmpCMDHandle)(AMFDecoder &dec);
	static unordered_map<string, rtmpCMDHandle> g_mapCmd;

	//超时功能实现
	std::shared_ptr<Timer> m_pPublishTimer;
    
    //源
    std::weak_ptr<RtmpMediaSource> m_pMediaSrc;
    RtmpMediaSource::RingType::RingReader::Ptr m_pRtmpReader;
    //事件监听
    Event m_onShutdown;
    Event m_onPublished;
};

} /* namespace Rtmp */
} /* namespace ZL */

#endif /* SRC_RTMP_RTMPPUSHER_H_ */
