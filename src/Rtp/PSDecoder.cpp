/*
 * Copyright (c) 2016 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/xia-chu/ZLMediaKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#if defined(ENABLE_RTPPROXY)
#include "PSDecoder.h"
#include "mpeg-ps.h"
namespace mediakit{

PSDecoder::PSDecoder() {
    _ps_demuxer = ps_demuxer_create([](void* param,
                                       int stream,
                                       int codecid,
                                       int flags,
                                       int64_t pts,
                                       int64_t dts,
                                       const void* data,
                                       size_t bytes){
        PSDecoder *thiz = (PSDecoder *)param;
        if(thiz->_on_decode){
            thiz->_on_decode(stream, codecid, flags, pts, dts, data, bytes);
        }
        return 0;
    },this);

    ps_demuxer_notify_t notify = {
            [](void *param, int stream, int codecid, const void *extra, int bytes, int finish) {
                PSDecoder *thiz = (PSDecoder *) param;
                if (thiz->_on_stream) {
                    thiz->_on_stream(stream, codecid, extra, bytes, finish);
                }
            }
    };
    ps_demuxer_set_notify((struct ps_demuxer_t *) _ps_demuxer, &notify, this);
}

PSDecoder::~PSDecoder() {
    ps_demuxer_destroy((struct ps_demuxer_t*)_ps_demuxer);
}

ssize_t PSDecoder::input(const uint8_t *data, size_t bytes) {
    HttpRequestSplitter::input(reinterpret_cast<const char *>(data), bytes);
    return bytes;
}

void PSDecoder::setOnDecode(Decoder::onDecode cb) {
    _on_decode = std::move(cb);
}

void PSDecoder::setOnStream(Decoder::onStream cb) {
    _on_stream = std::move(cb);
}

const char *PSDecoder::onSearchPacketTail(const char *data, size_t len) {
    try {
        auto ret = ps_demuxer_input(static_cast<struct ps_demuxer_t *>(_ps_demuxer), reinterpret_cast<const uint8_t *>(data), len);
        if (ret >= 0) {
            //解析成功全部或部分
            return data + ret;
        }

        //解析失败，丢弃所有数据
        return data + len;
    } catch (std::exception &ex) {
        InfoL << "解析 ps 异常: bytes=" << len
              << ", exception=" << ex.what()
              << ", hex=" << hexdump(data, MIN(len, 32));
        if (remainDataSize() > 256 * 1024) {
            //缓存太多数据无法处理则上抛异常
            throw;
        }

        return nullptr;
    }
}

}//namespace mediakit
#endif//#if defined(ENABLE_RTPPROXY)