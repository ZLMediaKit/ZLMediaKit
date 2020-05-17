/*
 * Copyright (c) 2016 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/xiongziliang/ZLMediaKit).
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
    typedef std::function<void(const char *data,uint64_t len)> onSegment;
    TSSegment(int size = TS_PACKET_SIZE) : _size(size){}
    ~TSSegment(){}
    void setOnSegment(const onSegment &cb);
    static bool isTSPacket(const char *data, int len);
protected:
    int64_t onRecvHeader(const char *data, uint64_t len) override ;
    const char *onSearchPacketTail(const char *data, int len) override ;
private:
    int _size;
    onSegment _onSegment;
};

#if defined(ENABLE_HLS)
//ts解析器
class TSDecoder : public Decoder {
public:
    TSDecoder();
    ~TSDecoder();
    int input(const uint8_t* data, int bytes) override ;
    void setOnDecode(const onDecode &decode) override;
private:
    TSSegment _ts_segment;
    struct ts_demuxer_t* _demuxer_ctx = nullptr;
    onDecode _on_decode;
};
#endif//defined(ENABLE_HLS)

}//namespace mediakit
#endif //ZLMEDIAKIT_TSDECODER_H
