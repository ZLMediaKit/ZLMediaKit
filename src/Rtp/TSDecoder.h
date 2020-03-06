/*
 * MIT License
 *
 * Copyright (c) 2020 xiongziliang <771730766@qq.com>
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

#ifndef ZLMEDIAKIT_TSDECODER_H
#define ZLMEDIAKIT_TSDECODER_H

#if defined(ENABLE_RTPPROXY)
#include "Util/logger.h"
#include "Http/HttpRequestSplitter.h"
#include "Decoder.h"

using namespace toolkit;
namespace mediakit {

//ts包拆分器
class TSSegment : public HttpRequestSplitter {
public:
    typedef std::function<void(const char *data,uint64_t len)> onSegment;
    TSSegment(int size = 188) : _size(size){}
    ~TSSegment(){}
    void setOnSegment(const onSegment &cb);
protected:
    int64_t onRecvHeader(const char *data, uint64_t len) override ;
    const char *onSearchPacketTail(const char *data, int len) override ;
private:
    int _size;
    onSegment _onSegment;
};

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

}//namespace mediakit
#endif//defined(ENABLE_RTPPROXY)
#endif //ZLMEDIAKIT_TSDECODER_H
