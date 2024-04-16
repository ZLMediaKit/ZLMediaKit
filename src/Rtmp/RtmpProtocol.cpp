/*
 * Copyright (c) 2016-present The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/ZLMediaKit/ZLMediaKit).
 *
 * Use of this source code is governed by MIT-like license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#include <algorithm>
#include "RtmpProtocol.h"
#include "Rtmp/utils.h"
#include "RtmpMediaSource.h"
#include "Util/util.h"

using namespace std;
using namespace toolkit;

#define C1_DIGEST_SIZE 32
#define C1_KEY_SIZE 128
#define C1_SCHEMA_SIZE 764
#define C1_HANDSHARK_SIZE (RANDOM_LEN + 8)
#define C1_FPKEY_SIZE 30
#define S1_FMS_KEY_SIZE 36
#define S2_FMS_KEY_SIZE 68
#define C1_OFFSET_SIZE 4

#ifdef ENABLE_OPENSSL
#include "Util/SSLBox.h"
#include <openssl/hmac.h>
#include <openssl/opensslv.h>

static string openssl_HMACsha256(const void *key, size_t key_len, const void *data, size_t data_len){
    std::shared_ptr<char> out(new char[32], [](char *ptr) { delete[] ptr; });
    unsigned int out_len;

#if defined(OPENSSL_VERSION_NUMBER) && (OPENSSL_VERSION_NUMBER > 0x10100000L)
    //openssl 1.1.0新增api，老版本api作废
    HMAC_CTX *ctx = HMAC_CTX_new();
    HMAC_CTX_reset(ctx);
    HMAC_Init_ex(ctx, key, (int)key_len, EVP_sha256(), NULL);
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

namespace mediakit {

RtmpProtocol::RtmpProtocol() {
    _packet_pool.setSize(64);
    _next_step_func = [this](const char *data, size_t len) {
        return handle_C0C1(data, len);
    };
}

RtmpProtocol::~RtmpProtocol() {
    reset();
}

void RtmpProtocol::reset() {
    ////////////ChunkSize////////////
    _chunk_size_in = DEFAULT_CHUNK_LEN;
    _chunk_size_out = DEFAULT_CHUNK_LEN;
    ////////////Acknowledgement////////////
    _bytes_sent = 0;
    _bytes_sent_last = 0;
    _windows_size = 0;
    ///////////PeerBandwidth///////////
    _bandwidth = 2500000;
    _band_limit_type = 2;
    ////////////Chunk////////////
    _map_chunk_data.clear();
    _now_stream_index = 0;
    _now_chunk_id = 0;
    //////////Invoke Request//////////
    _send_req_id = 0;
    //////////Rtmp parser//////////
    HttpRequestSplitter::reset();
    _stream_index = STREAM_CONTROL;
    _next_step_func = [this](const char *data, size_t len) {
        return handle_C0C1(data, len);
    };
}

void RtmpProtocol::sendAcknowledgement(uint32_t size) {
    size = htonl(size);
    std::string acknowledgement((char *) &size, 4);
    sendRequest(MSG_ACK, acknowledgement);
}

void RtmpProtocol::sendAcknowledgementSize(uint32_t size) {
    size = htonl(size);
    std::string set_windowSize((char *) &size, 4);
    sendRequest(MSG_WIN_SIZE, set_windowSize);
}

void RtmpProtocol::sendPeerBandwidth(uint32_t size) {
    size = htonl(size);
    std::string set_peerBandwidth((char *) &size, 4);
    set_peerBandwidth.push_back((char) 0x02);
    sendRequest(MSG_SET_PEER_BW, set_peerBandwidth);
}

void RtmpProtocol::sendChunkSize(uint32_t size) {
    uint32_t len = htonl(size);
    std::string set_chunk((char *) &len, 4);
    sendRequest(MSG_SET_CHUNK, set_chunk);
    _chunk_size_out = size;
}

void RtmpProtocol::sendPingRequest(uint32_t stamp) {
    sendUserControl(CONTROL_PING_REQUEST, stamp);
}

void RtmpProtocol::sendPingResponse(uint32_t time_stamp) {
    sendUserControl(CONTROL_PING_RESPONSE, time_stamp);
}

void RtmpProtocol::sendSetBufferLength(uint32_t stream_index, uint32_t len) {
    std::string control;
    stream_index = htonl(stream_index);
    control.append((char *) &stream_index, 4);

    len = htonl(len);
    control.append((char *) &len, 4);
    sendUserControl(CONTROL_SETBUFFER, control);
}

void RtmpProtocol::sendUserControl(uint16_t event_type, uint32_t event_data) {
    std::string control;
    event_type = htons(event_type);
    control.append((char *) &event_type, 2);

    event_data = htonl(event_data);
    control.append((char *) &event_data, 4);
    sendRequest(MSG_USER_CONTROL, control);
}

void RtmpProtocol::sendUserControl(uint16_t event_type, const string &event_data) {
    std::string control;
    event_type = htons(event_type);
    control.append((char *) &event_type, 2);
    control.append(event_data);
    sendRequest(MSG_USER_CONTROL, control);
}

void RtmpProtocol::sendResponse(int type, const string &str) {
    if(!_data_started && (type == MSG_DATA)){
        _data_started =  true;
    }
    sendRtmp(type, _now_stream_index, str, 0, _data_started ? CHUNK_CLIENT_REQUEST_AFTER : CHUNK_CLIENT_REQUEST_BEFORE);
}

void RtmpProtocol::sendInvoke(const string &cmd, const AMFValue &val) {
    AMFEncoder enc;
    enc << cmd << ++_send_req_id << val;
    sendRequest(MSG_CMD, enc.data());
}

void RtmpProtocol::sendRequest(int cmd, const string& str) {
    if (cmd <= MSG_SET_PEER_BW) {
        // 若 cmd 属于 Protocol Control Messages ，则应使用 chunk id 2 发送
        sendRtmp(cmd, _stream_index, str, 0, CHUNK_NETWORK);
    } else {
        // 否则使用 chunk id 发送(任意值3-128，参见 obs 及 ffmpeg 选取 3)
        sendRtmp(cmd, _stream_index, str, 0, CHUNK_SYSTEM);
    }
}

class BufferPartial : public Buffer {
public:
    BufferPartial(const Buffer::Ptr &buffer, size_t offset, size_t size) {
        _buffer = buffer;
        _data = buffer->data() + offset;
        _size = size;
    }

    char *data() const override {
        return _data;
    }

    size_t size() const override{
        return _size;
    }

private:
    char *_data;
    size_t _size;
    Buffer::Ptr _buffer;
};

void RtmpProtocol::sendRtmp(uint8_t type, uint32_t stream_index, const std::string &buffer, uint32_t stamp, int chunk_id) {
    sendRtmp(type, stream_index, std::make_shared<BufferString>(buffer), stamp, chunk_id);
}

void RtmpProtocol::sendRtmp(uint8_t type, uint32_t stream_index, const Buffer::Ptr &buf, uint32_t stamp, int chunk_id){
    if (chunk_id < 2 || chunk_id > 63) {
        auto strErr = StrPrinter << "不支持发送该类型的块流 ID:" << chunk_id << endl;
        throw std::runtime_error(strErr);
    }
    //是否有扩展时间戳
    bool ext_stamp = stamp >= 0xFFFFFF;

    //rtmp头
    BufferRaw::Ptr buffer_header = obtainBuffer();
    buffer_header->setCapacity(sizeof(RtmpHeader));
    buffer_header->setSize(sizeof(RtmpHeader));
    //对rtmp头赋值，如果使用整形赋值，在arm android上可能由于数据对齐导致总线错误的问题
    RtmpHeader *header = (RtmpHeader *) buffer_header->data();
    header->fmt = 0;
    header->chunk_id = chunk_id;
    header->type_id = type;
    set_be24(header->time_stamp, ext_stamp ? 0xFFFFFF : stamp);
    set_be24(header->body_size, (uint32_t)buf->size());
    set_le32(header->stream_index, stream_index);
    //发送rtmp头
    onSendRawData(std::move(buffer_header));

    //扩展时间戳字段
    BufferRaw::Ptr buffer_ext_stamp;
    if (ext_stamp) {
        //生成扩展时间戳
        buffer_ext_stamp = obtainBuffer();
        buffer_ext_stamp->setCapacity(4);
        buffer_ext_stamp->setSize(4);
        set_be32(buffer_ext_stamp->data(), stamp);
    }

    //生成一个字节的flag，标明是什么chunkId
    BufferRaw::Ptr buffer_flags = obtainBuffer();
    buffer_flags->setCapacity(1);
    buffer_flags->setSize(1);
    header = (RtmpHeader *) buffer_flags->data();
    header->fmt = 3;
    header->chunk_id = chunk_id;

    size_t offset = 0;
    size_t totalSize = sizeof(RtmpHeader);
    while (offset < buf->size()) {
        if (offset) {
            onSendRawData(buffer_flags);
            totalSize += 1;
        }
        if (ext_stamp) {
            //扩展时间戳
            onSendRawData(buffer_ext_stamp);
            totalSize += 4;
        }
        size_t chunk = min(_chunk_size_out, buf->size() - offset);
        onSendRawData(std::make_shared<BufferPartial>(buf, offset, chunk));
        totalSize += chunk;
        offset += chunk;
    }
    _bytes_sent += (uint32_t)totalSize;
    if (_windows_size > 0 && _bytes_sent - _bytes_sent_last >= _windows_size) {
        _bytes_sent_last = _bytes_sent;
        sendAcknowledgement(_bytes_sent);
    }
}

void RtmpProtocol::onParseRtmp(const char *data, size_t size) {
    input(data, size);
}

const char *RtmpProtocol::onSearchPacketTail(const char *data,size_t len){
    //移动拷贝提高性能
    auto next_step_func(std::move(_next_step_func));
    //执行下一步
    auto ret = next_step_func(data, len);
    if (!_next_step_func) {
        //为设置下一步，恢复之
        next_step_func.swap(_next_step_func);
    }
    return ret;
}

////for client////
void RtmpProtocol::startClientSession(const function<void()> &func, bool complex) {
    //发送 C0C1
    char handshake_head = HANDSHAKE_PLAINTEXT;
    onSendRawData(obtainBuffer(&handshake_head, 1));
    RtmpHandshake c1(0);
    if (complex) {
        c1.create_complex_c0c1();
    }
    onSendRawData(obtainBuffer((char *) (&c1), sizeof(c1)));
    _next_step_func = [this, func](const char *data, size_t len) {
        //等待 S0+S1+S2
        return handle_S0S1S2(data, len, func);
    };
}

const char* RtmpProtocol::handle_S0S1S2(const char *data, size_t len, const function<void()> &func) {
    if (len < 1 + 2 * C1_HANDSHARK_SIZE) {
        //数据不够
        return nullptr;
    }
    if (data[0] != HANDSHAKE_PLAINTEXT) {
        throw std::runtime_error("only plaintext[0x03] handshake supported");
    }
    //发送 C2
    const char *pcC2 = data + 1;
    onSendRawData(obtainBuffer(pcC2, C1_HANDSHARK_SIZE));
    //握手结束
    _next_step_func = [this](const char *data, size_t len) {
        //握手结束并且开始进入解析命令模式
        return handle_rtmp(data, len);
    };
    func();
    return data + 1 + 2 * C1_HANDSHARK_SIZE;
}

////for server ////
const char * RtmpProtocol::handle_C0C1(const char *data, size_t len) {
    if (len < 1 + C1_HANDSHARK_SIZE) {
        //need more data!
        return nullptr;
    }
    if (data[0] != HANDSHAKE_PLAINTEXT) {
        throw std::runtime_error("only plaintext[0x03] handshake supported");
    }
    if (memcmp(data + 5, "\x00\x00\x00\x00", 4) == 0) {
        //simple handsharke
        handle_C1_simple(data);
    } else {
#ifdef ENABLE_OPENSSL
        //complex handsharke
        handle_C1_complex(data);
#else
        WarnL << "未打开ENABLE_OPENSSL宏，复杂握手采用简单方式处理，flash播放器可能无法播放！";
        handle_C1_simple(data);
#endif//ENABLE_OPENSSL
    }
    return data + 1 + C1_HANDSHARK_SIZE;
}

void RtmpProtocol::handle_C1_simple(const char *data){
    //发送S0
    char handshake_head = HANDSHAKE_PLAINTEXT;
    onSendRawData(obtainBuffer(&handshake_head, 1));
    //发送S1
    RtmpHandshake s1(0);
    onSendRawData(obtainBuffer((char *) &s1, C1_HANDSHARK_SIZE));
    //发送S2
    onSendRawData(obtainBuffer(data + 1, C1_HANDSHARK_SIZE));
    //等待C2
    _next_step_func = [this](const char *data, size_t len) {
        //握手结束并且开始进入解析命令模式
        return handle_C2(data, len);
    };
}

#ifdef ENABLE_OPENSSL
void RtmpProtocol::handle_C1_complex(const char *data){
    //参考自:http://blog.csdn.net/win_lin/article/details/13006803
    //skip c0,time,version
    const char *c1_start = data + 1;
    const char *schema_start = c1_start + 8;
    char *digest_start;
    try {
        /* c1s1 schema0
        time: 4bytes
        version: 4bytes
        key: 764bytes
        digest: 764bytes
         */
        auto digest = get_C1_digest((uint8_t *) schema_start + C1_SCHEMA_SIZE, &digest_start);
        string c1_joined(c1_start, C1_HANDSHARK_SIZE);
        c1_joined.erase(digest_start - c1_start, C1_DIGEST_SIZE);
        check_C1_Digest(digest, c1_joined);

        send_complex_S0S1S2(0, digest);
//		InfoL << "schema0";
    } catch (std::exception &) {
        //貌似flash从来都不用schema1
//		WarnL << "try rtmp complex schema0 failed:" <<  ex.what();
        try {
            /* c1s1 schema1
            time: 4bytes
            version: 4bytes
            digest: 764bytes
            key: 764bytes
             */
            auto digest = get_C1_digest((uint8_t *) schema_start, &digest_start);
            string c1_joined(c1_start, C1_HANDSHARK_SIZE);
            c1_joined.erase(digest_start - c1_start, C1_DIGEST_SIZE);
            check_C1_Digest(digest, c1_joined);

            send_complex_S0S1S2(1, digest);
//			InfoL << "schema1";
        } catch (std::exception &) {
//			WarnL << "try rtmp complex schema1 failed:" <<  ex.what();
            handle_C1_simple(data);
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
    auto sha256 = openssl_HMACsha256(FPKey, C1_FPKEY_SIZE, data.data(), data.size());
    if (sha256 != digest) {
        throw std::runtime_error("digest mismatched");
    } else {
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
    for (int i = 0; i < C1_OFFSET_SIZE; ++i) {
        offset += ptr[i];
    }
    offset %= (C1_SCHEMA_SIZE - C1_DIGEST_SIZE - C1_OFFSET_SIZE);
    *digestPos = (char *) ptr + C1_OFFSET_SIZE + offset;
    string digest(*digestPos, C1_DIGEST_SIZE);
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
    for (int i = C1_SCHEMA_SIZE - C1_OFFSET_SIZE; i < C1_SCHEMA_SIZE; ++i) {
        offset += ptr[i];
    }
    offset %= (C1_SCHEMA_SIZE - C1_KEY_SIZE - C1_OFFSET_SIZE);
    string key((char *) ptr + offset, C1_KEY_SIZE);
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
    memcpy(s1.zero, "\x04\x05\x00\x01", 4);
    char *digestPos;
    if (schemeType == 0) {
        /* c1s1 schema0
        time: 4bytes
        version: 4bytes
        key: 764bytes
        digest: 764bytes
         */
        get_C1_digest(s1.random + C1_SCHEMA_SIZE, &digestPos);
    } else {
        /* c1s1 schema1
        time: 4bytes
        version: 4bytes
        digest: 764bytes
        key: 764bytes
         */
        get_C1_digest(s1.random, &digestPos);
    }
    char *s1_start = (char *) &s1;
    string s1_joined(s1_start, sizeof(s1));
    s1_joined.erase(digestPos - s1_start, C1_DIGEST_SIZE);
    string s1_digest = openssl_HMACsha256(FMSKey, S1_FMS_KEY_SIZE, s1_joined.data(), s1_joined.size());
    memcpy(digestPos, s1_digest.data(), s1_digest.size());
    onSendRawData(obtainBuffer((char *) &s1, sizeof(s1)));

    //S2
    string s2_key = openssl_HMACsha256(FMSKey, S2_FMS_KEY_SIZE, digest.data(), digest.size());
    RtmpHandshake s2(0);
    s2.random_generate((char *) &s2, 8);
    string s2_digest = openssl_HMACsha256(s2_key.data(), s2_key.size(), &s2, sizeof(s2) - C1_DIGEST_SIZE);
    memcpy((char *) &s2 + C1_HANDSHARK_SIZE - C1_DIGEST_SIZE, s2_digest.data(), C1_DIGEST_SIZE);
    onSendRawData(obtainBuffer((char *) &s2, sizeof(s2)));
    //等待C2
    _next_step_func = [this](const char *data, size_t len) {
        return handle_C2(data, len);
    };
}
#endif //ENABLE_OPENSSL

//发送复杂握手c0c1
void RtmpHandshake::create_complex_c0c1() {
#ifdef ENABLE_OPENSSL
    memcpy(zero, "\x80\x00\x07\x02", 4);
    //digest随机偏移长度
    auto offset_value = rand() % (C1_SCHEMA_SIZE - C1_OFFSET_SIZE - C1_DIGEST_SIZE);
    //设置digest偏移长度
    auto offset_ptr = random + C1_SCHEMA_SIZE;
    offset_ptr[0] = offset_ptr[1] = offset_ptr[2] = offset_value / 4;
    offset_ptr[3] = offset_value - 3 * (offset_value / 4);
    //去除digest后的剩余随机数据
    string str((char *) this, sizeof(*this));
    str.erase(8 + C1_SCHEMA_SIZE + C1_OFFSET_SIZE + offset_value, C1_DIGEST_SIZE);
    //获取摘要
    auto digest_value = openssl_HMACsha256(FPKey, C1_FPKEY_SIZE, str.data(), str.size());
    //插入摘要
    memcpy(random + C1_SCHEMA_SIZE + C1_OFFSET_SIZE + offset_value, digest_value.data(), digest_value.size());
#endif
}

const char* RtmpProtocol::handle_C2(const char *data, size_t len) {
    if (len < C1_HANDSHARK_SIZE) {
        //need more data!
        return nullptr;
    }
    _next_step_func = [this](const char *data, size_t len) {
        return handle_rtmp(data, len);
    };

    //握手结束，进入命令模式
    return handle_rtmp(data + C1_HANDSHARK_SIZE, len - C1_HANDSHARK_SIZE);
}

static constexpr size_t HEADER_LENGTH[] = {12, 8, 4, 1};

const char* RtmpProtocol::handle_rtmp(const char *data, size_t len) {
    auto ptr = data;
    while (len) {
        size_t offset = 0;
        auto header = (RtmpHeader *) ptr;
        auto header_len = HEADER_LENGTH[header->fmt];
        _now_chunk_id = header->chunk_id;
        switch (_now_chunk_id) {
            case 0: {
                //0 值表示二字节形式，并且 ID 范围 64 - 319
                //(第二个字节 + 64)。
                if (len < 2) {
                    //need more data
                    return ptr;
                }
                _now_chunk_id = 64 + (uint8_t) (ptr[1]);
                offset = 1;
                break;
            }

            case 1: {
                //1 值表示三字节形式，并且 ID 范围为 64 - 65599
                //((第三个字节) * 256 + 第二个字节 + 64)。
                if (len < 3) {
                    //need more data
                    return ptr;
                }
                _now_chunk_id = 64 + ((uint8_t) (ptr[2]) << 8) + (uint8_t) (ptr[1]);
                offset = 2;
                break;
            }

            //带有 2 值的块流 ID 被保留，用于下层协议控制消息和命令。
            default : break;
        }

        if (len < header_len + offset) {
            //need more data
            return ptr;
        }
        header = (RtmpHeader *) (ptr + offset);
        auto &pr = _map_chunk_data[_now_chunk_id];
        auto &now_packet = pr.first;
        auto &last_packet = pr.second;
        if (!now_packet) {
            now_packet = RtmpPacket::create();
            if (last_packet) {
                //恢复chunk上下文
                *now_packet = *last_packet;
            }
            //绝对时间戳标记复位
            now_packet->is_abs_stamp = false;
        }
        auto &chunk_data = *now_packet;
        chunk_data.chunk_id = _now_chunk_id;
        switch (header_len) {
            case 12:
                chunk_data.is_abs_stamp = true;
                chunk_data.stream_index = load_le32(header->stream_index);
            case 8:
                chunk_data.body_size = load_be24(header->body_size);
                chunk_data.type_id = header->type_id;
            case 4:
                chunk_data.ts_field = load_be24(header->time_stamp);
        }

        auto time_stamp = chunk_data.ts_field;
        if (chunk_data.ts_field == 0xFFFFFF) {
            if (len < header_len + offset + 4) {
                //need more data
                return ptr;
            }
            time_stamp = load_be32(ptr + offset + header_len);
            offset += 4;
        }

        if (chunk_data.body_size < chunk_data.buffer.size()) {
            throw std::runtime_error("非法的bodySize");
        }

        auto more = min(_chunk_size_in, (size_t) (chunk_data.body_size - chunk_data.buffer.size()));
        if (len < header_len + offset + more) {
            //need more data
            return ptr;
        }
        if (more) {
            chunk_data.buffer.append(ptr + header_len + offset, more);
        }
        ptr += header_len + offset + more;
        len -= header_len + offset + more;
        if (chunk_data.buffer.size() == chunk_data.body_size) {
            //frame is ready
            _now_stream_index = chunk_data.stream_index;
            chunk_data.time_stamp = time_stamp + (chunk_data.is_abs_stamp ? 0 : chunk_data.time_stamp);
            //保存chunk上下文
            last_packet = now_packet;
            if (chunk_data.body_size) {
                handle_chunk(std::move(now_packet));
            } else {
                now_packet = nullptr;
            }
        }
    }
    return ptr;
}

void RtmpProtocol::handle_chunk(RtmpPacket::Ptr packet) {
    auto &chunk_data = *packet;
    switch (chunk_data.type_id) {
        case MSG_ACK: {
            if (chunk_data.buffer.size() < 4) {
                throw std::runtime_error("MSG_ACK: Not enough data");
            }
            //auto bytePeerRecv = load_be32(&chunk_data.buffer[0]);
            //TraceL << "MSG_ACK:" << bytePeerRecv;
            break;
        }

        case MSG_SET_CHUNK: {
            if (chunk_data.buffer.size() < 4) {
                throw std::runtime_error("MSG_SET_CHUNK :Not enough data");
            }
            _chunk_size_in = load_be32(&chunk_data.buffer[0]);
            TraceL << "MSG_SET_CHUNK:" << _chunk_size_in;
            break;
        }

        case MSG_USER_CONTROL: {
            //user control message
            if (chunk_data.buffer.size() < 2) {
                throw std::runtime_error("MSG_USER_CONTROL: Not enough data.");
            }
            uint16_t event_type = load_be16(&chunk_data.buffer[0]);
            chunk_data.buffer.erase(0, 2);
            switch (event_type) {
                case CONTROL_PING_REQUEST: {
                    if (chunk_data.buffer.size() < 4) {
                        throw std::runtime_error("CONTROL_PING_REQUEST: Not enough data.");
                    }
                    uint32_t timeStamp = load_be32(&chunk_data.buffer[0]);
                    //TraceL << "CONTROL_PING_REQUEST:" << time_stamp;
                    sendUserControl(CONTROL_PING_RESPONSE, timeStamp);
                    break;
                }

                case CONTROL_PING_RESPONSE: {
                    if (chunk_data.buffer.size() < 4) {
                        throw std::runtime_error("CONTROL_PING_RESPONSE: Not enough data.");
                    }
                    //uint32_t time_stamp = load_be32(&chunk_data.buffer[0]);
                    //TraceL << "CONTROL_PING_RESPONSE:" << time_stamp;
                    break;
                }

                case CONTROL_STREAM_BEGIN: {
                    //开始播放
                    if (chunk_data.buffer.size() < 4) {
                        WarnL << "CONTROL_STREAM_BEGIN: Not enough data:" << chunk_data.buffer.size();
                        break;
                    }
                    uint32_t stream_index = load_be32(&chunk_data.buffer[0]);
                    onStreamBegin(stream_index);
                    TraceL << "CONTROL_STREAM_BEGIN:" << stream_index;
                    break;
                }

                case CONTROL_STREAM_EOF: {
                    //暂停
                    if (chunk_data.buffer.size() < 4) {
                        throw std::runtime_error("CONTROL_STREAM_EOF: Not enough data.");
                    }
                    uint32_t stream_index = load_be32(&chunk_data.buffer[0]);
                    onStreamEof(stream_index);
                    TraceL << "CONTROL_STREAM_EOF:" << stream_index;
                    break;
                }

                case CONTROL_STREAM_DRY: {
                    //停止播放
                    if (chunk_data.buffer.size() < 4) {
                        throw std::runtime_error("CONTROL_STREAM_DRY: Not enough data.");
                    }
                    uint32_t stream_index = load_be32(&chunk_data.buffer[0]);
                    onStreamDry(stream_index);
                    TraceL << "CONTROL_STREAM_DRY:" << stream_index;
                    break;
                }

                default: /*WarnL << "unhandled user control:" << event_type; */ break;
            }
            break;
        }

        case MSG_WIN_SIZE: {
            //如果窗口太小，会导致发送sendAcknowledgement时无限递归：https://github.com/ZLMediaKit/ZLMediaKit/issues/1839
            //窗口太大，也可能导致fms服务器认为播放器心跳超时
            _windows_size = min(max(load_be32(&chunk_data.buffer[0]), 32 * 1024U), 1280 * 1024U);
            TraceL << "MSG_WIN_SIZE:" << _windows_size;
            break;
        }

        case MSG_SET_PEER_BW: {
            _bandwidth = load_be32(&chunk_data.buffer[0]);
            _band_limit_type =  chunk_data.buffer[4];
            TraceL << "MSG_SET_PEER_BW:" << _bandwidth << " " << (int)_band_limit_type;
            break;
        }

        case MSG_AGGREGATE: {
            auto ptr = (uint8_t *) chunk_data.buffer.data();
            auto ptr_tail = ptr + chunk_data.buffer.size();
            uint32_t latest_ts, timestamp;
            timestamp = chunk_data.time_stamp;
            bool first_message = true;
            while (ptr + 8 + 3 < ptr_tail) {
                auto type = *ptr;
                ptr += 1;
                auto size = load_be24(ptr);
                ptr += 3;
                auto ts = load_be24(ptr);
                ptr += 3;
                ts |= (*ptr << 24);
                ptr += 1;
                ptr += 3;
                //参考FFmpeg多拷贝了4个字节
                size += 4;
                if (ptr + size > ptr_tail) {
                    break;
                }
                if (!first_message) {
                    timestamp += ts - latest_ts;
                }
                first_message = false;
                latest_ts = ts;
                auto sub_packet_ptr = RtmpPacket::create();
                auto &sub_packet = *sub_packet_ptr;
                sub_packet.buffer.assign((char *)ptr, size);
                sub_packet.type_id = type;
                sub_packet.body_size = size;
                sub_packet.time_stamp = timestamp;
                sub_packet.stream_index = chunk_data.stream_index;
                sub_packet.chunk_id = chunk_data.chunk_id;
                handle_chunk(std::move(sub_packet_ptr));
                ptr += size;
            }
            break;
        }

        default: {
            _bytes_recv += packet->size();
            if (_windows_size > 0 && _bytes_recv - _bytes_recv_last >= _windows_size) {
                _bytes_recv_last = _bytes_recv;
                sendAcknowledgement(_bytes_recv);
            }
            onRtmpChunk(std::move(packet));
            break;
        }
    }
}

BufferRaw::Ptr RtmpProtocol::obtainBuffer(const void *data, size_t len) {
    auto buffer = _packet_pool.obtain2();
    if (data && len) {
        buffer->assign((const char *) data, len);
    }
    return buffer;
}

} /* namespace mediakit */
