/*
 * RtmpProtocol.cpp
 *
 *  Created on: 2017年2月7日
 *      Author: xzl
 */

#include "RtmpProtocol.h"
#include "Rtsp/Rtsp.h"
#include "Rtmp/utils.h"
#include "Util/util.h"
#include "Util/onceToken.h"
#include "Thread/ThreadPool.h"

using namespace ZL::Util;

namespace ZL {
namespace Rtmp {

RtmpProtocol::RtmpProtocol() {
	m_nextHandle = [this](){
		handle_C0C1();
	};
}
RtmpProtocol::~RtmpProtocol() {
	clear();
}
void RtmpProtocol::clear() {
	////////////ChunkSize////////////
	m_iChunkLenIn = DEFAULT_CHUNK_LEN;
	m_iChunkLenOut = DEFAULT_CHUNK_LEN;
	////////////Acknowledgement////////////
	m_ui32ByteSent = 0;
	m_ui32LastSent = 0;
	m_ui32WinSize = 0;
	///////////PeerBandwidth///////////
	m_ui32Bandwidth = 2500000;
	m_ui8LimitType = 2;
	////////////Chunk////////////
	m_mapChunkData.clear();
	m_iNowStreamID = 0;
	m_iNowChunkID = 0;
	//////////Invoke Request//////////
	m_iReqID = 0;
	//////////Rtmp parser//////////
	m_strRcvBuf.clear();
	m_ui32StreamId = STREAM_CONTROL;
	m_nextHandle = [this]() {
		handle_C0C1();
	};
}

void RtmpProtocol::sendAcknowledgement(uint32_t ui32Size) {
	std::string control;
	uint32_t stream = htonl(ui32Size);
	control.append((char *) &stream, 4);
	sendRequest(MSG_ACK, control);
}

void RtmpProtocol::sendAcknowledgementSize(uint32_t ui32Size) {
	uint32_t windowSize = htonl(ui32Size);
	std::string set_windowSize((char *) &windowSize, 4);
	sendRequest(MSG_WIN_SIZE, set_windowSize);
}

void RtmpProtocol::sendPeerBandwidth(uint32_t ui32Size) {
	uint32_t peerBandwidth = htonl(ui32Size);
	std::string set_peerBandwidth((char *) &peerBandwidth, 4);
	set_peerBandwidth.push_back((char) 0x02);
	sendRequest(MSG_SET_PEER_BW, set_peerBandwidth);
}

void RtmpProtocol::sendChunkSize(uint32_t ui32Size) {
	uint32_t len = htonl(ui32Size);
	std::string set_chunk((char *) &len, 4);
	sendRequest(MSG_SET_CHUNK, set_chunk);
	m_iChunkLenOut = ui32Size;
}

void RtmpProtocol::sendPingRequest(uint32_t ui32TimeStamp) {
	sendUserControl(CONTROL_PING_REQUEST, ui32TimeStamp);
}

void RtmpProtocol::sendPingResponse(uint32_t ui32TimeStamp) {
	sendUserControl(CONTROL_PING_RESPONSE, ui32TimeStamp);
}

void RtmpProtocol::sendSetBufferLength(uint32_t ui32StreamId,
		uint32_t ui32Length) {
	std::string control;
	ui32StreamId = htonl(ui32StreamId);
	control.append((char *) &ui32StreamId, 4);
	ui32Length = htonl(ui32Length);
	control.append((char *) &ui32Length, 4);
	sendUserControl(CONTROL_SETBUFFER, control);
}

void RtmpProtocol::sendUserControl(uint16_t ui16EventType,
		uint32_t ui32EventData) {
	std::string control;
	uint16_t type = htons(ui16EventType);
	control.append((char *) &type, 2);
	uint32_t stream = htonl(ui32EventData);
	control.append((char *) &stream, 4);
	sendRequest(MSG_USER_CONTROL, control);
}

void RtmpProtocol::sendUserControl(uint16_t ui16EventType,
		const string& strEventData) {
	std::string control;
	uint16_t type = htons(ui16EventType);
	control.append((char *) &type, 2);
	control.append(strEventData);
	sendRequest(MSG_USER_CONTROL, control);
}

void RtmpProtocol::sendResponse(int iType, const string& str) {
	if(!m_bDataStarted && (iType == MSG_DATA)){
		m_bDataStarted =  true;
	}
	sendRtmp(iType, m_iNowStreamID, str, 0, m_bDataStarted ? CHUNK_CLIENT_REQUEST_AFTER : CHUNK_CLIENT_REQUEST_BEFORE);
}

void RtmpProtocol::sendInvoke(const string& strCmd, const AMFValue& val) {
	AMFEncoder enc;
	enc << strCmd << ++m_iReqID << val;
	sendRequest(MSG_CMD, enc.data());
}

void RtmpProtocol::sendRequest(int iCmd, const string& str) {
	sendRtmp(iCmd, m_ui32StreamId, str, 0, CHUNK_SERVER_REQUEST);
}

void RtmpProtocol::sendRtmp(uint8_t ui8Type, uint32_t ui32StreamId,
		const std::string& strBuf, uint32_t ui32TimeStamp, int iChunkId) {
	if (iChunkId < 2 || iChunkId > 63) {
		auto strErr = StrPrinter << "不支持发送该类型的块流 ID：" << iChunkId << endl;
		throw std::runtime_error(strErr);
	}

	bool bExtStamp = ui32TimeStamp >= 0xFFFFFF;
	RtmpHeader header;
	header.flags = (iChunkId & 0x3f) | (0 << 6);
	header.typeId = ui8Type;
	set_be24(header.timeStamp, bExtStamp ? 0xFFFFFF : ui32TimeStamp);
	set_be24(header.bodySize, strBuf.size());
	set_le32(header.streamId, ui32StreamId);
	std::string strSend;
	strSend.append((char *) &header, sizeof header);
	char acExtStamp[4];
	if (bExtStamp) {
		//扩展时间戳
		set_be32(acExtStamp, ui32TimeStamp);
	}
	size_t pos = 0;
	while (pos < strBuf.size()) {
		if (pos) {
			uint8_t flags = (iChunkId & 0x3f) | (3 << 6);
			strSend += char(flags);
		}
		if (bExtStamp) {
			//扩展时间戳
			strSend.append(acExtStamp, 4);
		}
		size_t chunk = min(m_iChunkLenOut, strBuf.size() - pos);
		strSend.append(strBuf, pos, chunk);
		pos += chunk;
	}
	onSendRawData(strSend.data(),strSend.size());
	m_ui32ByteSent += strSend.size();
	if (m_ui32WinSize > 0 && m_ui32ByteSent - m_ui32LastSent >= m_ui32WinSize) {
		m_ui32LastSent = m_ui32ByteSent;
		sendAcknowledgement(m_ui32ByteSent);
	}
}

void RtmpProtocol::onParseRtmp(const char *pcRawData, int iSize) {
	m_strRcvBuf.append(pcRawData, iSize);
	auto cb = m_nextHandle;
	cb();
}

////for client////
void RtmpProtocol::startClientSession(const function<void()> &callBack) {
	//发送 C0C1
	char handshake_head = HANDSHAKE_PLAINTEXT;
	onSendRawData(&handshake_head, 1);
	RtmpHandshake c0c1(::time(NULL));
	onSendRawData((char *) (&c0c1), sizeof(RtmpHandshake));
	m_nextHandle = [this,callBack]() {
		//等待 S0+S1+S2
		handle_S0S1S2(callBack);
	};
}
void RtmpProtocol::handle_S0S1S2(const function<void()> &callBack) {
	if (m_strRcvBuf.size() < 1 + 2 * sizeof(RtmpHandshake)) {
		//数据不够
		return;
	}
	if (m_strRcvBuf[0] != HANDSHAKE_PLAINTEXT) {
		throw std::runtime_error("only plaintext[0x03] handshake supported");
	}
	//发送 C2
	const char *pcC2 = m_strRcvBuf.data() + 1;
	onSendRawData(pcC2, sizeof(RtmpHandshake));
	m_strRcvBuf.erase(0, 1 + 2 * sizeof(RtmpHandshake));
	//握手结束
	m_nextHandle = [this]() {
		//握手结束并且开始进入解析命令模式
		handle_rtmp();
	};
	callBack();
}
////for server ////
void RtmpProtocol::handle_C0C1() {
	if (m_strRcvBuf.size() < 1 + sizeof(RtmpHandshake)) {
		//need more data!
		return;
	}
	if (m_strRcvBuf[0] != HANDSHAKE_PLAINTEXT) {
		throw std::runtime_error("only plaintext[0x03] handshake supported");
	}
	char handshake_head = HANDSHAKE_PLAINTEXT;
	//发送S0
	onSendRawData(&handshake_head, 1);
	//发送S1
	RtmpHandshake s2(0);
	onSendRawData((char *) &s2, sizeof(RtmpHandshake));
	//发送S2
	onSendRawData(m_strRcvBuf.c_str() + 1, sizeof(RtmpHandshake));
	m_strRcvBuf.erase(0, 1 + sizeof(RtmpHandshake));
	//等待C2
	m_nextHandle = [this]() {
		handle_C2();
	};
}

void RtmpProtocol::handle_C2() {
	if (m_strRcvBuf.size() < sizeof(RtmpHandshake)) {
		//need more data!
		return;
	}
	m_strRcvBuf.erase(0, sizeof(RtmpHandshake));
	//握手结束，进入命令模式
	if (!m_strRcvBuf.empty()) {
		handle_rtmp();
	}
	m_nextHandle = [this]() {
		handle_rtmp();
	};
}

void RtmpProtocol::handle_rtmp() {
	while (!m_strRcvBuf.empty()) {
		uint8_t flags = m_strRcvBuf[0];
		int iOffset = 0;
		static const size_t HEADER_LENGTH[] = { 12, 8, 4, 1 };
		size_t iHeaderLen = HEADER_LENGTH[flags >> 6];
		m_iNowChunkID = flags & 0x3f;
		switch (m_iNowChunkID) {
		case 0: {
			//0 值表示二字节形式，并且 ID 范围 64 - 319
			//(第二个字节 + 64)。
			if (m_strRcvBuf.size() < 2) {
				//need more data
				return;
			}
			m_iNowChunkID = 64 + (uint8_t) (m_strRcvBuf[1]);
			iOffset = 1;
		}
			break;
		case 1: {
			//1 值表示三字节形式，并且 ID 范围为 64 - 65599
			//((第三个字节) * 256 + 第二个字节 + 64)。
			if (m_strRcvBuf.size() < 3) {
				//need more data
				return;
			}
			m_iNowChunkID = 64 + ((uint8_t) (m_strRcvBuf[2]) << 8) + (uint8_t) (m_strRcvBuf[1]);
			iOffset = 2;
		}
			break;
		default:
			//带有 2 值的块流 ID 被保留，用于下层协议控制消息和命令。
			break;
		}

		if (m_strRcvBuf.size() < iHeaderLen + iOffset) {
			//need more data
			return;
		}
		RtmpHeader &header = *((RtmpHeader *) (m_strRcvBuf.data() + iOffset));
		auto &chunkData = m_mapChunkData[m_iNowChunkID];
		chunkData.chunkId = m_iNowChunkID;
		switch (iHeaderLen) {
		case 12:
			chunkData.streamId = load_le32(header.streamId);
		case 8:
			chunkData.bodySize = load_be24(header.bodySize);
			chunkData.typeId = header.typeId;
		case 4:
			uint32_t ts = load_be24(header.timeStamp);
			if (ts == 0xFFFFFF) {
				chunkData.extStamp = true;
			}else{
				chunkData.extStamp = false;
				chunkData.timeStamp = ts + ((iHeaderLen == 12) ? 0 : chunkData.timeStamp);
			}
		}
		if (chunkData.extStamp) {
			if (m_strRcvBuf.size() < iHeaderLen + iOffset + 4) {
				//need more data
				return;
			}
			chunkData.timeStamp = load_be32( m_strRcvBuf.data() + iOffset + iHeaderLen);
			iOffset += 4;
		}

		if (chunkData.bodySize == 0 || chunkData.bodySize < chunkData.strBuf.size()) {
			throw std::runtime_error("非法的bodySize");
		}

		auto iMore = min(m_iChunkLenIn, chunkData.bodySize - chunkData.strBuf.size());
		if (m_strRcvBuf.size() < iHeaderLen + iOffset + iMore) {
			//need more data
			return;
		}
		chunkData.strBuf.append(m_strRcvBuf, iHeaderLen + iOffset, iMore);
		m_strRcvBuf.erase(0, iHeaderLen + iOffset + iMore);
		if (chunkData.strBuf.size() == chunkData.bodySize) {
			m_iNowStreamID = chunkData.streamId;
			handle_rtmpChunk(chunkData);
			chunkData.strBuf.clear();
		}
	}
}

void RtmpProtocol::handle_rtmpChunk(RtmpPacket& chunkData) {
	switch (chunkData.typeId) {
		case MSG_ACK: {
			if (chunkData.strBuf.size() < 4) {
				throw std::runtime_error("MSG_ACK: Not enough data");
			}
			//auto bytePeerRecv = load_be32(&chunkData.strBuf[0]);
			//TraceL << "MSG_ACK:" << bytePeerRecv;
		}
			break;
		case MSG_SET_CHUNK: {
			if (chunkData.strBuf.size() < 4) {
				throw std::runtime_error("MSG_SET_CHUNK :Not enough data");
			}
			m_iChunkLenIn = load_be32(&chunkData.strBuf[0]);
			TraceL << "MSG_SET_CHUNK:" << m_iChunkLenIn;
		}
			break;
		case MSG_USER_CONTROL: {
			//user control message
			if (chunkData.strBuf.size() < 2) {
				throw std::runtime_error("MSG_USER_CONTROL: Not enough data.");
			}
			uint16_t event_type = load_be16(&chunkData.strBuf[0]);
			chunkData.strBuf.erase(0, 2);
			switch (event_type) {
			case CONTROL_PING_REQUEST: {
					if (chunkData.strBuf.size() < 4) {
						throw std::runtime_error("CONTROL_PING_REQUEST: Not enough data.");
					}
					uint32_t timeStamp = load_be32(&chunkData.strBuf[0]);
					//TraceL << "CONTROL_PING_REQUEST:" << timeStamp;
					sendUserControl(CONTROL_PING_RESPONSE, timeStamp);
				}
					break;
			case CONTROL_PING_RESPONSE: {
				if (chunkData.strBuf.size() < 4) {
					throw std::runtime_error("CONTROL_PING_RESPONSE: Not enough data.");
				}
				//uint32_t timeStamp = load_be32(&chunkData.strBuf[0]);
				//TraceL << "CONTROL_PING_RESPONSE:" << timeStamp;
			}
				break;
			case CONTROL_STREAM_BEGIN: {
				//开始播放
				if (chunkData.strBuf.size() < 4) {
					throw std::runtime_error("CONTROL_STREAM_BEGIN: Not enough data.");
				}
				uint32_t stramId = load_be32(&chunkData.strBuf[0]);
				onStreamBegin(stramId);
				TraceL << "CONTROL_STREAM_BEGIN:" << stramId;
			}
				break;

			case CONTROL_STREAM_EOF: {
				//暂停
				if (chunkData.strBuf.size() < 4) {
					throw std::runtime_error("CONTROL_STREAM_EOF: Not enough data.");
				}
				uint32_t stramId = load_be32(&chunkData.strBuf[0]);
				onStreamEof(stramId);
				TraceL << "CONTROL_STREAM_EOF:" << stramId;
			}
				break;
			case CONTROL_STREAM_DRY: {
				//停止播放
				if (chunkData.strBuf.size() < 4) {
					throw std::runtime_error("CONTROL_STREAM_DRY: Not enough data.");
				}
				uint32_t stramId = load_be32(&chunkData.strBuf[0]);
				onStreamDry(stramId);
				TraceL << "CONTROL_STREAM_DRY:" << stramId;
			}
				break;
			default:
				//WarnL << "unhandled user control:" << event_type;
				break;
			}
		}
			break;

		case MSG_WIN_SIZE: {
			m_ui32WinSize = load_be32(&chunkData.strBuf[0]);
			TraceL << "MSG_WIN_SIZE:" << m_ui32WinSize;
		}
			break;
		case MSG_SET_PEER_BW: {
			m_ui32Bandwidth = load_be32(&chunkData.strBuf[0]);
			m_ui8LimitType =  chunkData.strBuf[4];
			TraceL << "MSG_SET_PEER_BW:" << m_ui32WinSize;
		}
			break;
		case MSG_AGGREGATE:
			throw std::runtime_error("streaming FLV not supported");
			break;
		default:
			onRtmpChunk(chunkData);
			break;
		}
}

} /* namespace Rtmp */
} /* namespace ZL */
