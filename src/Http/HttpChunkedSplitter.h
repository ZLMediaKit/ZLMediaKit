/*
 * Copyright (c) 2016 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/xiongziliang/ZLMediaKit).
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
