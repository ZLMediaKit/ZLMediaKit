/*
 * Copyright (c) 2016 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/xia-chu/ZLMediaKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#include <string.h>
#include "Common/macros.h"
#include "HttpChunkedSplitter.h"

using namespace std;

//[chunk size][\r\n][chunk data][\r\n][chunk size][\r\n][chunk data][\r\n][chunk size = 0][\r\n][\r\n]

namespace mediakit{

const char *HttpChunkedSplitter::onSearchPacketTail(const char *data, size_t len) {
    auto pos = strstr(data, "\r\n");
    if (!pos) {
        return nullptr;
    }
    return pos + 2;
}

void HttpChunkedSplitter::onRecvContent(const char *data, size_t len) {
    onRecvChunk(data, len - 2);
}

ssize_t HttpChunkedSplitter::onRecvHeader(const char *data, size_t len) {
    int size;
    CHECK(sscanf(data, "%X", &size) == 1 && size >= 0);
    //包括后面\r\n两个字节
    return size + 2;
}

}//namespace mediakit
