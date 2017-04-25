/*
 * RtmpPusher.h
 *
 *  Created on: 2017年2月13日
 *      Author: xzl
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
	RtmpPusher(const char *strApp,const char *strStream);
	virtual ~RtmpPusher();

	void publish(const char* strUrl);
	void teardown();

protected:

	//for Tcpclient
	void onRecv(const Socket::Buffer::Ptr &pBuf) override;
	void onConnect(const SockException &err) override;
	void onErr(const SockException &ex) override;

	//fro RtmpProtocol
	void onRtmpChunk(RtmpPacket &chunkData) override;
	void onSendRawData(const char *pcRawData, int iSize) override {
		send(pcRawData, iSize);
	}

	virtual void onShutdown(const SockException &ex){}
	virtual void onPlayResult(const SockException &ex) {}
private:
	void _onShutdown(const SockException &ex) {
		WarnL << ex.getErrCode() << " " << ex.what();
		m_pPlayTimer.reset();
		onShutdown(ex);
	}
	void _onPlayResult(const SockException &ex) {
		WarnL << ex.getErrCode() << " " << ex.what();
		m_pPlayTimer.reset();
		onPlayResult(ex);
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
	inline void send_publish();
	inline void send_metaData();

	string m_strApp;
	string m_strStream;
	string m_strTcUrl;

	unordered_map<int, function<void(AMFDecoder &dec)> > m_mapOnResultCB;
	deque<function<void(AMFValue &dec)> > m_dqOnStatusCB;
	recursive_mutex m_mtxDeque;

	typedef void (RtmpPusher::*rtmpCMDHandle)(AMFDecoder &dec);
	static unordered_map<string, rtmpCMDHandle> g_mapCmd;

	//超时功能实现
	std::shared_ptr<Timer> m_pPlayTimer;
    
    //源
    std::weak_ptr<RtmpMediaSource> m_pMediaSrc;
    RtmpMediaSource::RingType::RingReader::Ptr m_pRtmpReader;
};

} /* namespace Rtmp */
} /* namespace ZL */

#endif /* SRC_RTMP_RTMPPUSHER_H_ */
