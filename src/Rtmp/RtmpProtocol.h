/*
 * MIT License
 *
 * Copyright (c) 2016-2019 xiongziliang <771730766@qq.com>
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

#ifndef SRC_RTMP_RTMPPROTOCOL_H_
#define SRC_RTMP_RTMPPROTOCOL_H_

#include <memory>
#include <string>
#include <functional>
#include "amf.h"
#include "Rtmp.h"
#include "Util/util.h"
#include "Util/logger.h"
#include "Util/TimeTicker.h"
#include "Network/Socket.h"
#include "Util/ResourcePool.h"

using namespace std;
using namespace toolkit;

namespace mediakit {

class RtmpProtocol {
public:
	RtmpProtocol();
	virtual ~RtmpProtocol();
	//作为客户端发送c0c1，等待s0s1s2并且回调
	void startClientSession(const function<void()> &cb);
	void onParseRtmp(const char *pcRawData,int iSize);
	void reset();
protected:
	virtual void onSendRawData(const Buffer::Ptr &buffer) = 0;
	virtual void onRtmpChunk(RtmpPacket &chunkData) = 0;
	virtual void onStreamBegin(uint32_t ui32StreamId){
		_ui32StreamId = ui32StreamId;
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
	void sendRtmp(uint8_t ui8Type, uint32_t ui32StreamId, const Buffer::Ptr &buffer, uint32_t ui32TimeStamp, int iChunkID);
protected:
	int _iReqID = 0;
	uint32_t _ui32StreamId = STREAM_CONTROL;
	int _iNowStreamID = 0;
	int _iNowChunkID = 0;
	bool _bDataStarted = false;
	inline BufferRaw::Ptr obtainBuffer();
	inline BufferRaw::Ptr obtainBuffer(const void *data, int len);
	//ResourcePool<BufferRaw,MAX_SEND_PKT> _bufferPool;
private:
	void handle_S0S1S2(const function<void()> &cb);
	void handle_C0C1();
	void handle_C1_simple();
#ifdef ENABLE_OPENSSL
	void handle_C1_complex();
	string get_C1_digest(const uint8_t *ptr,char **digestPos);
	string get_C1_key(const uint8_t *ptr);
	void check_C1_Digest(const string &digest,const string &data);
	void send_complex_S0S1S2(int schemeType,const string &digest);
#endif //ENABLE_OPENSSL

	void handle_C2();
	void handle_rtmp();
	void handle_rtmpChunk(RtmpPacket &chunkData);

private:
	////////////ChunkSize////////////
	size_t _iChunkLenIn = DEFAULT_CHUNK_LEN;
	size_t _iChunkLenOut = DEFAULT_CHUNK_LEN;
	////////////Acknowledgement////////////
	uint32_t _ui32ByteSent = 0;
	uint32_t _ui32LastSent = 0;
	uint32_t _ui32WinSize = 0;
	///////////PeerBandwidth///////////
	uint32_t _ui32Bandwidth = 2500000;
	uint8_t _ui8LimitType = 2;
	////////////Chunk////////////
	unordered_map<int, RtmpPacket> _mapChunkData;
	//////////Rtmp parser//////////
	string _strRcvBuf;
	function<void()> _nextHandle;
};

} /* namespace mediakit */

#endif /* SRC_RTMP_RTMPPROTOCOL_H_ */
