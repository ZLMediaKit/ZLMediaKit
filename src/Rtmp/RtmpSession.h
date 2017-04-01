/*
 * RtmpSession.h
 *
 *  Created on: 2017年2月10日
 *      Author: xzl
 */

#ifndef SRC_RTMP_RTMPSESSION_H_
#define SRC_RTMP_RTMPSESSION_H_

#include <netinet/in.h>
#include <unordered_map>

#include "amf.h"
#include "Rtmp.h"
#include "utils.h"
#include "config.h"
#include "Util/util.h"
#include "Util/TimeTicker.h"
#include "RtmpProtocol.h"
#include "RtmpToRtspMediaSource.h"
#include "Network/TcpLimitedSession.h"

using namespace ZL::Util;
using namespace ZL::Network;

namespace ZL {
namespace Rtmp {

class RtmpSession: public TcpLimitedSession<MAX_TCP_SESSION> ,public  RtmpProtocol{
public:
	typedef std::shared_ptr<RtmpSession> Ptr;
	RtmpSession(const std::shared_ptr<ThreadPool> &_th, const Socket::Ptr &_sock);
	virtual ~RtmpSession();
	void onRecv(const Socket::Buffer::Ptr &pBuf) override;
	void onError(const SockException &err) override;
	void onManager() override;
private:
	std::string m_strApp;
	std::string m_strId;
	double m_dNowReqID = 0;
	Ticker m_ticker;//数据接收时间
	typedef void (RtmpSession::*rtmpCMDHandle)(AMFDecoder &dec);
	static unordered_map<string, rtmpCMDHandle> g_mapCmd;

	RingBuffer<RtmpPacket>::RingReader::Ptr m_pRingReader;
	std::shared_ptr<RtmpMediaSource> m_pPublisherSrc;
    std::weak_ptr<RtmpMediaSource> m_pPlayerSrc;

	void onProcessCmd(AMFDecoder &dec);
	void onCmd_connect(AMFDecoder &dec);
	void onCmd_createStream(AMFDecoder &dec);

	void onCmd_publish(AMFDecoder &dec);
	void onCmd_deleteStream(AMFDecoder &dec);

	void onCmd_play(AMFDecoder &dec);
	void onCmd_seek(AMFDecoder &dec);
	void onCmd_pause(AMFDecoder &dec);
	void setMetaData(AMFDecoder &dec);

	void onSendMedia(const RtmpPacket &pkt);
	void onSendRawData(const char *pcRawData,int iSize) override{
		send(pcRawData, iSize);
	}
	void onRtmpChunk(RtmpPacket &chunkData) override;

	template<typename first, typename second>
	inline void sendReply(const char *str, const first &reply, const second &status) {
		AMFEncoder invoke;
		invoke << str << m_dNowReqID << reply << status;
		sendResponse(MSG_CMD, invoke.data());
	}
};

} /* namespace Rtmp */
} /* namespace ZL */

#endif /* SRC_RTMP_RTMPSESSION_H_ */
