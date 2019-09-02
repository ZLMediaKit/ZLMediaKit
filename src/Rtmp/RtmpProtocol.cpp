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
#include "RtmpProtocol.h"
#include "Rtmp/utils.h"
#include "Util/util.h"
#include "Util/onceToken.h"
#include "Thread/ThreadPool.h"
using namespace toolkit;

#ifdef ENABLE_OPENSSL
#include "Util/SSLBox.h"
#include <openssl/hmac.h>
#include <openssl/opensslv.h>

static string openssl_HMACsha256(const void *key,unsigned int key_len,
								 const void *data,unsigned int data_len){
	std::shared_ptr<char> out(new char[32],[](char *ptr){delete [] ptr;});
	unsigned int out_len;

#if defined(OPENSSL_VERSION_NUMBER) && (OPENSSL_VERSION_NUMBER > 0x10100000L)
    //openssl 1.1.0新增api，老版本api作废
	HMAC_CTX *ctx = HMAC_CTX_new();
	HMAC_CTX_reset(ctx);
	HMAC_Init_ex(ctx, key, key_len, EVP_sha256(), NULL);
	HMAC_Update(ctx, (unsigned char*)data, data_len);
	HMAC_Final(ctx, (unsigned char *)out.get(), &out_len);
	HMAC_CTX_reset(ctx);
	HMAC_CTX_free(ctx);
#else
	HMAC_CTX ctx;
	HMAC_CTX_init(&ctx);
	HMAC_Init_ex(&ctx, key, key_len, EVP_sha256(), NULL);
	HMAC_Update(&ctx, (unsigned char*)data, data_len);
	HMAC_Final(&ctx, (unsigned char *)out.get(), &out_len);
	HMAC_CTX_cleanup(&ctx);
#endif //defined(OPENSSL_VERSION_NUMBER) && (OPENSSL_VERSION_NUMBER > 0x10100000L)
	return string(out.get(),out_len);
}
#endif //ENABLE_OPENSSL


#define C1_DIGEST_SIZE 32
#define C1_KEY_SIZE 128
#define C1_SCHEMA_SIZE 764
#define C1_HANDSHARK_SIZE (RANDOM_LEN + 8)
#define C1_FPKEY_SIZE 30
#define S1_FMS_KEY_SIZE 36
#define S2_FMS_KEY_SIZE 68
#define C1_OFFSET_SIZE 4

namespace mediakit {

RtmpProtocol::RtmpProtocol() {
	_nextHandle = [this](){
		handle_C0C1();
	};
}
RtmpProtocol::~RtmpProtocol() {
	reset();
}
void RtmpProtocol::reset() {
	////////////ChunkSize////////////
	_iChunkLenIn = DEFAULT_CHUNK_LEN;
	_iChunkLenOut = DEFAULT_CHUNK_LEN;
	////////////Acknowledgement////////////
	_ui32ByteSent = 0;
	_ui32LastSent = 0;
	_ui32WinSize = 0;
	///////////PeerBandwidth///////////
	_ui32Bandwidth = 2500000;
	_ui8LimitType = 2;
	////////////Chunk////////////
	_mapChunkData.clear();
	_iNowStreamID = 0;
	_iNowChunkID = 0;
	//////////Invoke Request//////////
	_iReqID = 0;
	//////////Rtmp parser//////////
	_strRcvBuf.clear();
	_ui32StreamId = STREAM_CONTROL;
	_nextHandle = [this]() {
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
	_iChunkLenOut = ui32Size;
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
	if(!_bDataStarted && (iType == MSG_DATA)){
		_bDataStarted =  true;
	}
	sendRtmp(iType, _iNowStreamID, str, 0, _bDataStarted ? CHUNK_CLIENT_REQUEST_AFTER : CHUNK_CLIENT_REQUEST_BEFORE);
}

void RtmpProtocol::sendInvoke(const string& strCmd, const AMFValue& val) {
	AMFEncoder enc;
	enc << strCmd << ++_iReqID << val;
	sendRequest(MSG_CMD, enc.data());
}

void RtmpProtocol::sendRequest(int iCmd, const string& str) {
	sendRtmp(iCmd, _ui32StreamId, str, 0, CHUNK_SERVER_REQUEST);
}

class BufferPartial : public Buffer {
public:
    BufferPartial(const Buffer::Ptr &buffer,uint32_t offset,uint32_t size){
        _buffer = buffer;
        _data = buffer->data() + offset;
        _size = size;
    }

    ~BufferPartial(){}

    char *data() const override {
        return _data;
    }
    uint32_t size() const override{
        return _size;
    }
private:
    Buffer::Ptr _buffer;
    char *_data;
    uint32_t _size;
};

void RtmpProtocol::sendRtmp(uint8_t ui8Type, uint32_t ui32StreamId,
                            const std::string& strBuf, uint32_t ui32TimeStamp, int iChunkId) {
    sendRtmp(ui8Type,ui32StreamId,std::make_shared<BufferString>(strBuf),ui32TimeStamp,iChunkId);
}

void RtmpProtocol::sendRtmp(uint8_t ui8Type, uint32_t ui32StreamId,
        const Buffer::Ptr &buf, uint32_t ui32TimeStamp, int iChunkId){
    if (iChunkId < 2 || iChunkId > 63) {
        auto strErr = StrPrinter << "不支持发送该类型的块流 ID:" << iChunkId << endl;
        throw std::runtime_error(strErr);
    }
	//是否有扩展时间戳
    bool bExtStamp = ui32TimeStamp >= 0xFFFFFF;

    //rtmp头
	BufferRaw::Ptr bufferHeader = obtainBuffer();
	bufferHeader->setCapacity(sizeof(RtmpHeader));
	bufferHeader->setSize(sizeof(RtmpHeader));
	//对rtmp头赋值，如果使用整形赋值，在arm android上可能由于数据对齐导致总线错误的问题
	RtmpHeader *header = (RtmpHeader*) bufferHeader->data();
    header->flags = (iChunkId & 0x3f) | (0 << 6);
    header->typeId = ui8Type;
    set_be24(header->timeStamp, bExtStamp ? 0xFFFFFF : ui32TimeStamp);
    set_be24(header->bodySize, buf->size());
    set_le32(header->streamId, ui32StreamId);
    //发送rtmp头
    onSendRawData(bufferHeader);

    //扩展时间戳字段
	BufferRaw::Ptr bufferExtStamp;
    if (bExtStamp) {
        //生成扩展时间戳
		bufferExtStamp = obtainBuffer();
		bufferExtStamp->setCapacity(4);
		bufferExtStamp->setSize(4);
		set_be32(bufferExtStamp->data(), ui32TimeStamp);
	}

	//生成一个字节的flag，标明是什么chunkId
	BufferRaw::Ptr bufferFlags = obtainBuffer();
	bufferFlags->setCapacity(1);
	bufferFlags->setSize(1);
	bufferFlags->data()[0] = (iChunkId & 0x3f) | (3 << 6);
    
    size_t offset = 0;
	uint32_t totalSize = sizeof(RtmpHeader);
    while (offset < buf->size()) {
        if (offset) {
            onSendRawData(bufferFlags);
            totalSize += 1;
        }
        if (bExtStamp) {
            //扩展时间戳
            onSendRawData(bufferExtStamp);
            totalSize += 4;
        }
        size_t chunk = min(_iChunkLenOut, buf->size() - offset);
        onSendRawData(std::make_shared<BufferPartial>(buf,offset,chunk));
        totalSize += chunk;
        offset += chunk;
    }
    _ui32ByteSent += totalSize;
    if (_ui32WinSize > 0 && _ui32ByteSent - _ui32LastSent >= _ui32WinSize) {
        _ui32LastSent = _ui32ByteSent;
        sendAcknowledgement(_ui32ByteSent);
    }
}


void RtmpProtocol::onParseRtmp(const char *pcRawData, int iSize) {
	_strRcvBuf.append(pcRawData, iSize);
	auto cb = _nextHandle;
	cb();
}

////for client////
void RtmpProtocol::startClientSession(const function<void()> &callBack) {
	//发送 C0C1
	char handshake_head = HANDSHAKE_PLAINTEXT;
	onSendRawData(obtainBuffer(&handshake_head, 1));
	RtmpHandshake c1(0);
	onSendRawData(obtainBuffer((char *) (&c1), sizeof(c1)));
	_nextHandle = [this,callBack]() {
		//等待 S0+S1+S2
		handle_S0S1S2(callBack);
	};
}
void RtmpProtocol::handle_S0S1S2(const function<void()> &callBack) {
	if (_strRcvBuf.size() < 1 + 2 * C1_HANDSHARK_SIZE) {
		//数据不够
		return;
	}
	if (_strRcvBuf[0] != HANDSHAKE_PLAINTEXT) {
		throw std::runtime_error("only plaintext[0x03] handshake supported");
	}
	//发送 C2
	const char *pcC2 = _strRcvBuf.data() + 1;
	onSendRawData(obtainBuffer(pcC2, C1_HANDSHARK_SIZE));
	_strRcvBuf.erase(0, 1 + 2 * C1_HANDSHARK_SIZE);
	//握手结束
	_nextHandle = [this]() {
		//握手结束并且开始进入解析命令模式
		handle_rtmp();
	};
	callBack();
}
////for server ////
void RtmpProtocol::handle_C0C1() {
	if (_strRcvBuf.size() < 1 + C1_HANDSHARK_SIZE) {
		//need more data!
		return;
	}
	if (_strRcvBuf[0] != HANDSHAKE_PLAINTEXT) {
		throw std::runtime_error("only plaintext[0x03] handshake supported");
	}
	if(memcmp(_strRcvBuf.data() + 5,"\x00\x00\x00\x00",4) ==0 ){
		//simple handsharke
		handle_C1_simple();
	}else{
#ifdef ENABLE_OPENSSL
		//complex handsharke
		handle_C1_complex();
#else
		WarnL << "未打开ENABLE_OPENSSL宏，复杂握手采用简单方式处理！";
		handle_C1_simple();
#endif//ENABLE_OPENSSL
	}
	_strRcvBuf.erase(0, 1 + C1_HANDSHARK_SIZE);
}
void RtmpProtocol::handle_C1_simple(){
	//发送S0
	char handshake_head = HANDSHAKE_PLAINTEXT;
	onSendRawData(obtainBuffer(&handshake_head, 1));
	//发送S1
	RtmpHandshake s1(0);
	onSendRawData(obtainBuffer((char *) &s1, C1_HANDSHARK_SIZE));
	//发送S2
	onSendRawData(obtainBuffer(_strRcvBuf.data() + 1, C1_HANDSHARK_SIZE));
	//等待C2
	_nextHandle = [this]() {
		handle_C2();
	};
}
#ifdef ENABLE_OPENSSL
void RtmpProtocol::handle_C1_complex(){
	//参考自:http://blog.csdn.net/win_lin/article/details/13006803
	//skip c0,time,version
	const char *c1_start = _strRcvBuf.data() + 1;
	const char *schema_start = c1_start + 8;
	char *digest_start;
	try{
		/* c1s1 schema0
		time: 4bytes
		version: 4bytes
		key: 764bytes
		digest: 764bytes
		 */
		auto digest = get_C1_digest((uint8_t *)schema_start + C1_SCHEMA_SIZE,&digest_start);
		string c1_joined(c1_start,C1_HANDSHARK_SIZE);
		c1_joined.erase(digest_start - c1_start , C1_DIGEST_SIZE );
		check_C1_Digest(digest,c1_joined);

		send_complex_S0S1S2(0,digest);
		InfoL << "schema0";
	}catch(std::exception &ex){
		//貌似flash从来都不用schema1
		WarnL << "try rtmp complex schema0 failed:" <<  ex.what();
		try{
			/* c1s1 schema1
			time: 4bytes
			version: 4bytes
			digest: 764bytes
			key: 764bytes
			 */
			auto digest = get_C1_digest((uint8_t *)schema_start,&digest_start);
			string c1_joined(c1_start,C1_HANDSHARK_SIZE);
			c1_joined.erase(digest_start - c1_start , C1_DIGEST_SIZE );
			check_C1_Digest(digest,c1_joined);

			send_complex_S0S1S2(1,digest);
			InfoL << "schema1";
		}catch(std::exception &ex){
			WarnL << "try rtmp complex schema1 failed:" <<  ex.what();
			handle_C1_simple();
		}
	}
}

#if !defined(u_int8_t)
#define u_int8_t unsigned char
#endif // !defined(u_int8_t)

static u_int8_t FMSKey[] = {
    0x47, 0x65, 0x6e, 0x75, 0x69, 0x6e, 0x65, 0x20,
    0x41, 0x64, 0x6f, 0x62, 0x65, 0x20, 0x46, 0x6c,
    0x61, 0x73, 0x68, 0x20, 0x4d, 0x65, 0x64, 0x69,
    0x61, 0x20, 0x53, 0x65, 0x72, 0x76, 0x65, 0x72,
    0x20, 0x30, 0x30, 0x31, // Genuine Adobe Flash Media Server 001
    0xf0, 0xee, 0xc2, 0x4a, 0x80, 0x68, 0xbe, 0xe8,
    0x2e, 0x00, 0xd0, 0xd1, 0x02, 0x9e, 0x7e, 0x57,
    0x6e, 0xec, 0x5d, 0x2d, 0x29, 0x80, 0x6f, 0xab,
    0x93, 0xb8, 0xe6, 0x36, 0xcf, 0xeb, 0x31, 0xae
}; // 68

static u_int8_t FPKey[] = {
    0x47, 0x65, 0x6E, 0x75, 0x69, 0x6E, 0x65, 0x20,
    0x41, 0x64, 0x6F, 0x62, 0x65, 0x20, 0x46, 0x6C,
    0x61, 0x73, 0x68, 0x20, 0x50, 0x6C, 0x61, 0x79,
    0x65, 0x72, 0x20, 0x30, 0x30, 0x31, // Genuine Adobe Flash Player 001
    0xF0, 0xEE, 0xC2, 0x4A, 0x80, 0x68, 0xBE, 0xE8,
    0x2E, 0x00, 0xD0, 0xD1, 0x02, 0x9E, 0x7E, 0x57,
    0x6E, 0xEC, 0x5D, 0x2D, 0x29, 0x80, 0x6F, 0xAB,
    0x93, 0xB8, 0xE6, 0x36, 0xCF, 0xEB, 0x31, 0xAE
}; // 62
void RtmpProtocol::check_C1_Digest(const string &digest,const string &data){
	auto sha256 = openssl_HMACsha256(FPKey,C1_FPKEY_SIZE,data.data(),data.size());
	if(sha256 != digest){
		throw std::runtime_error("digest mismatched");
	}else{
		InfoL << "check rtmp complex handshark success!";
	}
}
string RtmpProtocol::get_C1_digest(const uint8_t *ptr,char **digestPos){
	/* 764bytes digest结构
	offset: 4bytes
	random-data: (offset)bytes
	digest-data: 32bytes
	random-data: (764-4-offset-32)bytes
	 */
	int offset = 0;
	for(int i=0;i<C1_OFFSET_SIZE;++i){
		offset += ptr[i];
	}
	offset %= (C1_SCHEMA_SIZE - C1_DIGEST_SIZE - C1_OFFSET_SIZE);
	*digestPos = (char *)ptr + C1_OFFSET_SIZE + offset;
	string digest(*digestPos,C1_DIGEST_SIZE);
	//DebugL << "digest offset:" << offset << ",digest:" << hexdump(digest.data(),digest.size());
	return digest;
}
string RtmpProtocol::get_C1_key(const uint8_t *ptr){
	/* 764bytes key结构
	random-data: (offset)bytes
	key-data: 128bytes
	random-data: (764-offset-128-4)bytes
	offset: 4bytes
	 */
	int offset = 0;
	for(int i = C1_SCHEMA_SIZE - C1_OFFSET_SIZE;i< C1_SCHEMA_SIZE;++i){
		offset += ptr[i];
	}
	offset %= (C1_SCHEMA_SIZE - C1_KEY_SIZE - C1_OFFSET_SIZE);
	string key((char *)ptr + offset,C1_KEY_SIZE);
	//DebugL << "key offset:" << offset << ",key:" << hexdump(key.data(),key.size());
	return key;
}
void RtmpProtocol::send_complex_S0S1S2(int schemeType,const string &digest){
	//S1S2计算参考自:https://github.com/hitYangfei/golang/blob/master/rtmpserver.go
	//发送S0
	char handshake_head = HANDSHAKE_PLAINTEXT;
	onSendRawData(obtainBuffer(&handshake_head, 1));
	//S1
	RtmpHandshake s1(0);
	memcpy(s1.zero,"\x04\x05\x00\x01",4);
	char *digestPos;
	if(schemeType == 0){
		/* c1s1 schema0
		time: 4bytes
		version: 4bytes
		key: 764bytes
		digest: 764bytes
		 */
		get_C1_digest(s1.random + C1_SCHEMA_SIZE,&digestPos);
	}else{
		/* c1s1 schema1
		time: 4bytes
		version: 4bytes
		digest: 764bytes
		key: 764bytes
		 */
		get_C1_digest(s1.random,&digestPos);
	}
	char *s1_start = (char *)&s1;
	string s1_joined(s1_start,sizeof(s1));
	s1_joined.erase(digestPos - s1_start,C1_DIGEST_SIZE);
	string s1_digest = openssl_HMACsha256(FMSKey,S1_FMS_KEY_SIZE,s1_joined.data(),s1_joined.size());
	memcpy(digestPos,s1_digest.data(),s1_digest.size());
	onSendRawData(obtainBuffer((char *) &s1, sizeof(s1)));

	//S2
	string s2_key = openssl_HMACsha256(FMSKey,S2_FMS_KEY_SIZE,digest.data(),digest.size());
	RtmpHandshake s2(0);
	s2.random_generate((char *)&s2,8);
	string s2_digest = openssl_HMACsha256(s2_key.data(),s2_key.size(),&s2,sizeof(s2) - C1_DIGEST_SIZE);
	memcpy((char *)&s2 + C1_HANDSHARK_SIZE - C1_DIGEST_SIZE,s2_digest.data(),C1_DIGEST_SIZE);
	onSendRawData(obtainBuffer((char *)&s2, sizeof(s2)));
	//等待C2
	_nextHandle = [this]() {
		handle_C2();
	};
}
#endif //ENABLE_OPENSSL
void RtmpProtocol::handle_C2() {
	if (_strRcvBuf.size() < C1_HANDSHARK_SIZE) {
		//need more data!
		return;
	}
	_strRcvBuf.erase(0, C1_HANDSHARK_SIZE);
	//握手结束，进入命令模式
	if (!_strRcvBuf.empty()) {
		handle_rtmp();
	}
	_nextHandle = [this]() {
		handle_rtmp();
	};
}

void RtmpProtocol::handle_rtmp() {
	while (!_strRcvBuf.empty()) {
		uint8_t flags = _strRcvBuf[0];
		int iOffset = 0;
		static const size_t HEADER_LENGTH[] = { 12, 8, 4, 1 };
		size_t iHeaderLen = HEADER_LENGTH[flags >> 6];
		_iNowChunkID = flags & 0x3f;
		switch (_iNowChunkID) {
		case 0: {
			//0 值表示二字节形式，并且 ID 范围 64 - 319
			//(第二个字节 + 64)。
			if (_strRcvBuf.size() < 2) {
				//need more data
				return;
			}
			_iNowChunkID = 64 + (uint8_t) (_strRcvBuf[1]);
			iOffset = 1;
		}
			break;
		case 1: {
			//1 值表示三字节形式，并且 ID 范围为 64 - 65599
			//((第三个字节) * 256 + 第二个字节 + 64)。
			if (_strRcvBuf.size() < 3) {
				//need more data
				return;
			}
			_iNowChunkID = 64 + ((uint8_t) (_strRcvBuf[2]) << 8) + (uint8_t) (_strRcvBuf[1]);
			iOffset = 2;
		}
			break;
		default:
			//带有 2 值的块流 ID 被保留，用于下层协议控制消息和命令。
			break;
		}

		if (_strRcvBuf.size() < iHeaderLen + iOffset) {
			//need more data
			return;
		}
		RtmpHeader &header = *((RtmpHeader *) (_strRcvBuf.data() + iOffset));
		auto &chunkData = _mapChunkData[_iNowChunkID];
		chunkData.chunkId = _iNowChunkID;
		switch (iHeaderLen) {
		case 12:
            chunkData.hasAbsStamp = true;
			chunkData.streamId = load_le32(header.streamId);
		case 8:
			chunkData.bodySize = load_be24(header.bodySize);
			chunkData.typeId = header.typeId;
		case 4:
			chunkData.deltaStamp = load_be24(header.timeStamp);
            chunkData.hasExtStamp = chunkData.deltaStamp == 0xFFFFFF;
		}
		
        if (chunkData.hasExtStamp) {
			if (_strRcvBuf.size() < iHeaderLen + iOffset + 4) {
				//need more data
				return;
			}
            chunkData.deltaStamp = load_be32(_strRcvBuf.data() + iOffset + iHeaderLen);
			iOffset += 4;
		}
		
        if (chunkData.bodySize < chunkData.strBuf.size()) {
			throw std::runtime_error("非法的bodySize");
		}
        
		auto iMore = min(_iChunkLenIn, chunkData.bodySize - chunkData.strBuf.size());
		if (_strRcvBuf.size() < iHeaderLen + iOffset + iMore) {
			//need more data
			return;
		}
		
        chunkData.strBuf.append(_strRcvBuf, iHeaderLen + iOffset, iMore);
		_strRcvBuf.erase(0, iHeaderLen + iOffset + iMore);
        
		if (chunkData.strBuf.size() == chunkData.bodySize) {
            //frame is ready
            _iNowStreamID = chunkData.streamId;
            chunkData.timeStamp = chunkData.deltaStamp + (chunkData.hasAbsStamp ? 0 : chunkData.timeStamp);
            
			if(chunkData.bodySize){
				handle_rtmpChunk(chunkData);
			}
			chunkData.strBuf.clear();
            chunkData.hasAbsStamp = false;
            chunkData.hasExtStamp = false;
            chunkData.deltaStamp = 0;
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
			_iChunkLenIn = load_be32(&chunkData.strBuf[0]);
			TraceL << "MSG_SET_CHUNK:" << _iChunkLenIn;
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
			_ui32WinSize = load_be32(&chunkData.strBuf[0]);
			TraceL << "MSG_WIN_SIZE:" << _ui32WinSize;
		}
			break;
		case MSG_SET_PEER_BW: {
			_ui32Bandwidth = load_be32(&chunkData.strBuf[0]);
			_ui8LimitType =  chunkData.strBuf[4];
			TraceL << "MSG_SET_PEER_BW:" << _ui32WinSize;
		}
			break;
		case MSG_AGGREGATE: {
			auto ptr = (uint8_t*)chunkData.strBuf.data();
			auto ptr_tail = ptr + chunkData.strBuf.length() ;
			while(ptr + 8 + 3 < ptr_tail){
				auto type = *ptr;
				ptr += 1;
				auto size = load_be24(ptr);
				ptr += 3;
				auto ts = load_be24(ptr);
				ptr += 3;
				ts |= (*ptr << 24);
				ptr += 1;

				//参考ffmpeg忽略了3个字节
				/**
				 *  while (next - pkt->data < pkt->size - RTMP_HEADER) {
						type = bytestream_get_byte(&next);
						size = bytestream_get_be24(&next);
						cts  = bytestream_get_be24(&next);
						cts |= bytestream_get_byte(&next) << 24;
						if (!pts)
							pts = cts;
						ts += cts - pts;
						pts = cts;
						if (size + 3 + 4 > pkt->data + pkt->size - next)
							break;
						bytestream_put_byte(&p, type);
						bytestream_put_be24(&p, size);
						bytestream_put_be24(&p, ts);
						bytestream_put_byte(&p, ts >> 24);
						memcpy(p, next, size + 3 + 4);
						p    += size + 3;
						bytestream_put_be32(&p, size + RTMP_HEADER);
						next += size + 3 + 4;
					}
				 */
				ptr += 3;
				//参考FFmpeg多拷贝了4个字节
				size += 4;
				if(ptr + size > ptr_tail){
//				    ErrorL << ptr + size << " " << ptr_tail << " " << ptr_tail - ptr - size;
					break;
				}
//				DebugL << (int)type << " " << size << " " << ts << " " << chunkData.timeStamp << " " << ptr_tail - ptr;
				RtmpPacket sub_packet ;
				sub_packet.strBuf.resize(size);
				memcpy((char *)sub_packet.strBuf.data(),ptr,size);
				sub_packet.typeId = type;
				sub_packet.bodySize = size;
				sub_packet.timeStamp = ts;
				sub_packet.streamId = chunkData.streamId;
				sub_packet.chunkId = chunkData.chunkId;
				handle_rtmpChunk(sub_packet);
				ptr += size;
			}
//			InfoL << ptr_tail - ptr;
		}
			break;
		default:
			onRtmpChunk(chunkData);
			break;
		}
}

BufferRaw::Ptr RtmpProtocol::obtainBuffer() {
    return std::make_shared<BufferRaw>() ;//_bufferPool.obtain();
}

BufferRaw::Ptr RtmpProtocol::obtainBuffer(const void *data, int len) {
	auto buffer = obtainBuffer();
	buffer->assign((const char *)data,len);
	return buffer;
}

} /* namespace mediakit */
