/*
 * RtmpProtocol.h
 *
 *  Created on: 2017年2月7日
 *      Author: xzl
 */

#ifndef SRC_RTMP_RTMPPROTOCOL_H_
#define SRC_RTMP_RTMPPROTOCOL_H_

#include <netinet/in.h>
#include <memory>
#include <string>
#include <functional>
#include "amf.h"
#include "Rtmp.h"
#include "Util/util.h"
#include "Util/logger.h"
#include "Util/TimeTicker.h"
#include "Network/Socket.h"

using namespace std;
using namespace ZL::Util;
using namespace ZL::Network;

namespace ZL {
namespace Rtmp {

class RtmpProtocol {
public:
	RtmpProtocol();
	virtual ~RtmpProtocol();
	//作为客户端发送c0c1，等待s0s1s2并且回调
	void startClientSession(const function<void()> &cb);
	void onParseRtmp(const char *pcRawData,int iSize);
	void clear();
protected:
	virtual void onSendRawData(const char *pcRawData,int iSize) = 0;
	virtual void onRtmpChunk(RtmpPacket &chunkData) = 0;

	virtual void onStreamBegin(uint32_t ui32StreamId){
		m_ui32StreamId = ui32StreamId;
	}
	virtual void onStreamEof(uint32_t ui32StreamId){};
	virtual void onStreamDry(uint32_t ui32StreamId){};
protected:
	void sendAcknowledgement(uint32_t ui32Size);
	void sendAcknowledgementSize(uint32_t ui32Size);
	void sendPeerBandwidth(uint32_t ui32Size);
	void sendChunkSize(uint32_t ui32Size);
	void sendPingRequest(uint32_t ui32TimeStamp = ::time(NULL));
	void sendPingResponse(uint32_t ui32TimeStamp = ::time(NULL));
	void sendSetBufferLength(uint32_t ui32StreamId, uint32_t ui32Length);
	void sendUserControl(uint16_t ui16EventType, uint32_t ui32EventData);
	void sendUserControl(uint16_t ui16EventType, const string &strEventData);

	void sendInvoke(const string &strCmd, const AMFValue &val);
	void sendRequest(int iCmd, const string &str);
	void sendResponse(int iType, const string &str);
	void sendRtmp(uint8_t ui8Type, uint32_t ui32StreamId, const std::string &strBuf, uint32_t ui32TimeStamp, int iChunkID);
protected:
	int m_iReqID = 0;
	uint32_t m_ui32StreamId = STREAM_CONTROL;
	int m_iNowStreamID = 0;
	int m_iNowChunkID = 0;
	bool m_bDataStarted = false;
private:
	void handle_S0S1S2(const function<void()> &cb);
	void handle_C0C1();
	void handle_C2();
	void handle_rtmp();
	void handle_rtmpChunk(RtmpPacket &chunkData);

	////////////ChunkSize////////////
	size_t m_iChunkLenIn = DEFAULT_CHUNK_LEN;
	size_t m_iChunkLenOut = DEFAULT_CHUNK_LEN;
	////////////Acknowledgement////////////
	uint32_t m_ui32ByteSent = 0;
	uint32_t m_ui32LastSent = 0;
	uint32_t m_ui32WinSize = 0;
	///////////PeerBandwidth///////////
	uint32_t m_ui32Bandwidth = 2500000;
	uint8_t m_ui8LimitType = 2;
	////////////Chunk////////////
	unordered_map<int, RtmpPacket> m_mapChunkData;
	//////////Rtmp parser//////////
	string m_strRcvBuf;
	function<void()> m_nextHandle;
};

} /* namespace Rtmp */
} /* namespace ZL */

#endif /* SRC_RTMP_RTMPPROTOCOL_H_ */
