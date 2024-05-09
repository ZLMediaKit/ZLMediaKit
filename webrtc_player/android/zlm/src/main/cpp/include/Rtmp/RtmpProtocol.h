/*
 * Copyright (c) 2016-present The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/ZLMediaKit/ZLMediaKit).
 *
 * Use of this source code is governed by MIT-like license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#ifndef SRC_RTMP_RTMPPROTOCOL_H_
#define SRC_RTMP_RTMPPROTOCOL_H_

#include <memory>
#include <string>
#include <functional>
#include <unordered_map>
#include "amf.h"
#include "Rtmp.h"
#include "Util/ResourcePool.h"
#include "Http/HttpRequestSplitter.h"

namespace mediakit {

class RtmpProtocol : public HttpRequestSplitter{
public:
    RtmpProtocol();
    virtual ~RtmpProtocol();

    void onParseRtmp(const char *data, size_t size);
    //作为客户端发送c0c1，等待s0s1s2并且回调
    void startClientSession(const std::function<void()> &cb, bool complex = true);

protected:
    virtual void onSendRawData(toolkit::Buffer::Ptr buffer) = 0;
    virtual void onRtmpChunk(RtmpPacket::Ptr chunk_data) = 0;
    virtual void onStreamBegin(uint32_t stream_index){
        _stream_index = stream_index;
    }
    virtual void onStreamEof(uint32_t stream_index){};
    virtual void onStreamDry(uint32_t stream_index){};

protected:
    //// HttpRequestSplitter override ////
    ssize_t onRecvHeader(const char *data, size_t len) override { return 0; }
    const char *onSearchPacketTail(const char *data, size_t len) override;

protected:
    void reset();
    void sendAcknowledgement(uint32_t size);
    void sendAcknowledgementSize(uint32_t size);
    void sendPeerBandwidth(uint32_t size);
    void sendChunkSize(uint32_t size);
    void sendPingRequest(uint32_t ti = ::time(NULL));
    void sendPingResponse(uint32_t time_stamp = ::time(NULL));
    void sendSetBufferLength(uint32_t stream_index, uint32_t len);
    void sendUserControl(uint16_t event_type, uint32_t event_data);
    void sendUserControl(uint16_t event_type, const std::string &event_data);
    void sendInvoke(const std::string &cmd, const AMFValue &val);
    void sendRequest(int cmd, const std::string &str);
    void sendResponse(int type, const std::string &str);
    void sendRtmp(uint8_t type, uint32_t stream_index, const std::string &buffer, uint32_t stamp, int chunk_id);
    void sendRtmp(uint8_t type, uint32_t stream_index, const toolkit::Buffer::Ptr &buffer, uint32_t stamp, int chunk_id);
    toolkit::BufferRaw::Ptr obtainBuffer(const void *data = nullptr, size_t len = 0);

private:
    void handle_C1_simple(const char *data);
#ifdef ENABLE_OPENSSL
    void handle_C1_complex(const char *data);
    std::string get_C1_digest(const uint8_t *ptr,char **digestPos);
    std::string get_C1_key(const uint8_t *ptr);
    void check_C1_Digest(const std::string &digest,const std::string &data);
    void send_complex_S0S1S2(int schemeType,const std::string &digest);
#endif //ENABLE_OPENSSL

    const char* handle_S0S1S2(const char *data, size_t len, const std::function<void()> &func);
    const char* handle_C0C1(const char *data, size_t len);
    const char* handle_C2(const char *data, size_t len);
    const char* handle_rtmp(const char *data, size_t len);
    void handle_chunk(RtmpPacket::Ptr chunk_data);

protected:
    int _send_req_id = 0;
    int _now_stream_index = 0;
    uint32_t _stream_index = STREAM_CONTROL;

private:
    bool _data_started = false;
    int _now_chunk_id = 0;
    ////////////ChunkSize////////////
    size_t _chunk_size_in = DEFAULT_CHUNK_LEN;
    size_t _chunk_size_out = DEFAULT_CHUNK_LEN;
    ////////////Acknowledgement////////////
    uint64_t _bytes_sent = 0;
    uint64_t _bytes_sent_last = 0;
    uint64_t _bytes_recv = 0;
    uint64_t _bytes_recv_last = 0;
    uint32_t _windows_size = 0;
    ///////////PeerBandwidth///////////
    uint32_t _bandwidth = 2500000;
    uint8_t _band_limit_type = 2;
    //////////Rtmp parser//////////
    std::function<const char * (const char *data, size_t len)> _next_step_func;
    ////////////Chunk////////////
    std::unordered_map<int, std::pair<RtmpPacket::Ptr/*now*/, RtmpPacket::Ptr/*last*/> > _map_chunk_data;
    //循环池
    toolkit::ResourcePool<toolkit::BufferRaw> _packet_pool;
};

} /* namespace mediakit */
#endif /* SRC_RTMP_RTMPPROTOCOL_H_ */
