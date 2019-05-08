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
#ifndef ZLMEDIAKIT_HTTPCHUNKEDSPLITTER_H
#define ZLMEDIAKIT_HTTPCHUNKEDSPLITTER_H

#include <functional>
#include "HttpRequestSplitter.h"

namespace mediakit{

class HttpChunkedSplitter : public HttpRequestSplitter {
public:
    /**
     * len == 0时代表结束
     */
    typedef std::function<void (const char *data,uint64_t len)> onChunkData;

    HttpChunkedSplitter(const onChunkData &cb){
        _onChunkData = cb;
    };
    ~HttpChunkedSplitter() override {} ;
protected:
    int64_t onRecvHeader(const char *data,uint64_t len) override;
    void onRecvContent(const char *data,uint64_t len) override;
    const char *onSearchPacketTail(const char *data,int len) override;
protected:
    virtual void onRecvChunk(const char *data,uint64_t len){
        if(_onChunkData){
            _onChunkData(data,len);
        }
    };
private:
    onChunkData _onChunkData;
};

}//namespace mediakit
#endif //ZLMEDIAKIT_HTTPCHUNKEDSPLITTER_H
