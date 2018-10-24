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

namespace mediakit {

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
		_onPublished = onPublished;
	}

	void setOnShutdown(Event onShutdown) {
		_onShutdown = onShutdown;
	}

protected:
	//for Tcpclient override
	void onRecv(const Buffer::Ptr &pBuf) override;
	void onConnect(const SockException &err) override;
	void onErr(const SockException &ex) override;

	//for RtmpProtocol override
	void onRtmpChunk(RtmpPacket &chunkData) override;
	void onSendRawData(const Buffer::Ptr &buffer) override{
		send(buffer);
	}
private:
    void init(const RtmpMediaSource::Ptr  &src);
	void onShutdown(const SockException &ex) {
		_pPublishTimer.reset();
		if(_onShutdown){
			_onShutdown(ex);
		}
		_pRtmpReader.reset();
	}
	void onPublishResult(const SockException &ex) {
		_pPublishTimer.reset();
		if(_onPublished){
			_onPublished(ex);
		}
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
	inline void send_publish();
	inline void send_metaData();

	string _strApp;
	string _strStream;
	string _strTcUrl;

	unordered_map<int, function<void(AMFDecoder &dec)> > _mapOnResultCB;
	recursive_mutex _mtxOnResultCB;
	deque<function<void(AMFValue &dec)> > _dqOnStatusCB;
	recursive_mutex _mtxOnStatusCB;

	typedef void (RtmpPusher::*rtmpCMDHandle)(AMFDecoder &dec);
	static unordered_map<string, rtmpCMDHandle> g_mapCmd;

	//超时功能实现
	std::shared_ptr<Timer> _pPublishTimer;
    
    //源
    std::weak_ptr<RtmpMediaSource> _pMediaSrc;
    RtmpMediaSource::RingType::RingReader::Ptr _pRtmpReader;
    //事件监听
    Event _onShutdown;
    Event _onPublished;
};

} /* namespace mediakit */

#endif /* SRC_RTMP_RTMPPUSHER_H_ */
