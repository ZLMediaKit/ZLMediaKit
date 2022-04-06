/*
 * Copyright (c) 2016 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/xia-chu/ZLMediaKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
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
   using onChunkData = std::function<void(const char *data, size_t len)>;

    HttpChunkedSplitter(const onChunkData &cb) { _onChunkData = cb; };
    ~HttpChunkedSplitter() override { _onChunkData = nullptr; };

protected:
    ssize_t onRecvHeader(const char *data,size_t len) override;
    void onRecvContent(const char *data,size_t len) override;
    const char *onSearchPacketTail(const char *data,size_t len) override;

protected:
    virtual void onRecvChunk(const char *data,size_t len){
        if(_onChunkData){
            _onChunkData(data,len);
        }
    };

private:
    onChunkData _onChunkData;
};

}//namespace mediakit
#endif //ZLMEDIAKIT_HTTPCHUNKEDSPLITTER_H
