/*
 * Copyright (c) 2016 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/xia-chu/ZLMediaKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#include "TSDecoder.h"
namespace mediakit {

bool TSSegment::isTSPacket(const char *data, size_t len){
    return len == TS_PACKET_SIZE && ((uint8_t*)data)[0] == TS_SYNC_BYTE;
}

void TSSegment::setOnSegment(TSSegment::onSegment cb) {
    _onSegment = std::move(cb);
}

ssize_t TSSegment::onRecvHeader(const char *data, size_t len) {
    if (!isTSPacket(data, len)) {
        WarnL << "不是ts包:" << (int) (data[0]) << " " << len;
        return 0;
    }
    _onSegment(data, len);
    return 0;
}

const char *TSSegment::onSearchPacketTail(const char *data, size_t len) {
    if (len < _size + 1) {
        if (len == _size && ((uint8_t *) data)[0] == TS_SYNC_BYTE) {
            return data + _size;
        }
        return nullptr;
    }
    //下一个包头
    if (((uint8_t *) data)[_size] == TS_SYNC_BYTE) {
        return data + _size;
    }
    auto pos = memchr(data + _size, TS_SYNC_BYTE, len - _size);
    if (pos) {
        return (char *) pos;
    }
    if (remainDataSize() > 4 * _size) {
        //数据这么多都没ts包，全部清空
        return data + len;
    }
    //等待更多数据
    return nullptr;
}

////////////////////////////////////////////////////////////////

#if defined(ENABLE_HLS)
#include "mpeg-ts.h"
#include "mpeg-ts-proto.h"
TSDecoder::TSDecoder() : _ts_segment() {
    _ts_segment.setOnSegment([this](const char *data, size_t len){
        ts_demuxer_input(_demuxer_ctx,(uint8_t*)data,len);
    });
    _demuxer_ctx = ts_demuxer_create([](void* param, int program, int stream, int codecid, int flags, int64_t pts, int64_t dts, const void* data, size_t bytes){
        TSDecoder *thiz = (TSDecoder*)param;
        if (thiz->_on_decode) {
            if (flags & MPEG_FLAG_PACKET_CORRUPT) {
                WarnL << "ts packet lost, dts:" << dts << " pts:" << pts << " bytes:" << bytes;
            } else {
                thiz->_on_decode(stream, codecid, flags, pts, dts, data, bytes);
            }
        }
        return 0;
    },this);

    ts_demuxer_notify_t notify = {
            [](void *param, int stream, int codecid, const void *extra, int bytes, int finish) {
                TSDecoder *thiz = (TSDecoder *) param;
                if (thiz->_on_stream) {
                    thiz->_on_stream(stream, codecid, extra, bytes, finish);
                }
            }
    };
    ts_demuxer_set_notify((struct ts_demuxer_t *) _demuxer_ctx, &notify, this);
}

TSDecoder::~TSDecoder() {
    ts_demuxer_destroy(_demuxer_ctx);
}

ssize_t TSDecoder::input(const uint8_t *data, size_t bytes) {
    if (TSSegment::isTSPacket((char *)data, bytes)) {
        return ts_demuxer_input(_demuxer_ctx, (uint8_t *) data, bytes);
    }
    try {
        _ts_segment.input((char *) data, bytes);
    } catch (...) {
        //ts解析失败，清空缓存数据
        _ts_segment.reset();
        throw;
    }
    return bytes;
}

#endif//defined(ENABLE_HLS)

}//namespace mediakit
