/*
 * Copyright (c) 2016 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/xia-chu/ZLMediaKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#ifndef ZLMEDIAKIT_TSDECODER_H
#define ZLMEDIAKIT_TSDECODER_H

#include "Util/logger.h"
#include "Http/HttpRequestSplitter.h"
#include "Decoder.h"

using namespace toolkit;
namespace mediakit {

#define TS_PACKET_SIZE		188
#define TS_SYNC_BYTE        0x47

//TS包分割器，用于split一个一个的ts包
class TSSegment : public HttpRequestSplitter {
public:
    typedef std::function<void(const char *data,size_t len)> onSegment;
    TSSegment(size_t size = TS_PACKET_SIZE) : _size(size){}
    ~TSSegment(){}
    void setOnSegment(const onSegment &cb);
    static bool isTSPacket(const char *data, size_t len);

protected:
    ssize_t onRecvHeader(const char *data, size_t len) override ;
    const char *onSearchPacketTail(const char *data, size_t len) override ;

private:
    size_t _size;
    onSegment _onSegment;
};

#if defined(ENABLE_HLS)
//ts解析器
class TSDecoder : public Decoder {
public:
    TSDecoder();
    ~TSDecoder();
    ssize_t input(const uint8_t* data, size_t bytes) override ;
    void setOnDecode(onDecode cb) override;
    void setOnStream(onStream cb) override;

private:
    TSSegment _ts_segment;
    struct ts_demuxer_t* _demuxer_ctx = nullptr;
    onDecode _on_decode;
    onStream _on_stream;
};
#endif//defined(ENABLE_HLS)

}//namespace mediakit
#endif //ZLMEDIAKIT_TSDECODER_H
