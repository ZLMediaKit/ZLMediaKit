//
// Created by xzl on 2018/11/13.
//

#include "HttpChunkedSplitter.h"

namespace mediakit{
    
const char *HttpChunkedSplitter::onSearchPacketTail(const char *data, int len) {
    auto pos = strstr(data,"\r\n");
    if(!pos){
        return nullptr;
    }
    return pos + 2;
}

void HttpChunkedSplitter::onRecvContent(const char *data, uint64_t len) {
    onRecvChunk(data,len - 2);
}

int64_t HttpChunkedSplitter::onRecvHeader(const char *data, uint64_t len) {
    string str(data,len - 2);
    int ret;
    sscanf(str.data(),"%X",&ret);
    return ret + 2;
}

}//namespace mediakit