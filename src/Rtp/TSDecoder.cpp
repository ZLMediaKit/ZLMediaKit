/*
 * Copyright (c) 2016 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/xiongziliang/ZLMediaKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#if defined(ENABLE_RTPPROXY)
#include "mpeg-ts.h"
#include "TSDecoder.h"
#define TS_PACKET_SIZE 188
namespace mediakit {

void TSSegment::setOnSegment(const TSSegment::onSegment &cb) {
    _onSegment = cb;
}

int64_t TSSegment::onRecvHeader(const char *data, uint64_t len) {
    _onSegment(data, len);
    return 0;
}

const char *TSSegment::onSearchPacketTail(const char *data, int len) {
    if (len < _size + 1) {
        if (len == _size && ((uint8_t *) data)[0] == 0x47) {
            return data + _size;
        }
        return nullptr;
    }
    //下一个包头
    if (((uint8_t *) data)[_size] == 0x47) {
        return data + _size;
    }

    auto pos = memchr(data + _size, 0x47, len - _size);
    if (pos) {
        return (char *) pos;
    }
    return nullptr;
}

////////////////////////////////////////////////////////////////

TSDecoder::TSDecoder() : _ts_segment(TS_PACKET_SIZE) {
    _ts_segment.setOnSegment([this](const char *data,uint64_t len){
        if(((uint8_t*)data)[0] != 0x47 || len != TS_PACKET_SIZE ){
            WarnL << "不是ts包:" << (int)(data[0]) << " " << len;
            return;
        }
        ts_demuxer_input(_demuxer_ctx,(uint8_t*)data,len);
    });
    _demuxer_ctx = ts_demuxer_create([](void* param, int program, int stream, int codecid, int flags, int64_t pts, int64_t dts, const void* data, size_t bytes){
        TSDecoder *thiz = (TSDecoder*)param;
        if(thiz->_on_decode){
            thiz->_on_decode(stream,codecid,flags,pts,dts,data,bytes);
        }
        return 0;
    },this);
}

TSDecoder::~TSDecoder() {
    ts_demuxer_destroy(_demuxer_ctx);
}

int TSDecoder::input(const uint8_t *data, int bytes) {
    if(bytes == TS_PACKET_SIZE && ((uint8_t*)data)[0] == 0x47){
        return ts_demuxer_input(_demuxer_ctx,(uint8_t*)data,bytes);
    }
    _ts_segment.input((char*)data,bytes);
    return bytes;
}

void TSDecoder::setOnDecode(const Decoder::onDecode &decode) {
    _on_decode = decode;
}

}//namespace mediakit
#endif//defined(ENABLE_RTPPROXY)