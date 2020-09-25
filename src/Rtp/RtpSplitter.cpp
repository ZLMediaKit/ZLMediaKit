/*
 * Copyright (c) 2016 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/xiongziliang/ZLMediaKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#if defined(ENABLE_RTPPROXY)
#include "RtpSplitter.h"
#include <string.h>
namespace mediakit{

RtpSplitter::RtpSplitter() {}

RtpSplitter::~RtpSplitter() {}

const char *RtpSplitter::onSearchPacketTail(const char *data, int len) {
    if (len < 256) {
        //数据不够
        return nullptr;
    }
    char hikHeader[4] = {0x01,0x00,0x01,0x00};

    if (!strncmp(hikHeader, data, 4)) {
        data = data + 256;
        len = len - 256;
    }
    if (data[0] == '$') {
        //可能是4个字节的rtp头
        return onSearchPacketTail_l(data + 2, len - 2);
    }
    //两个字节的rtp头
    return onSearchPacketTail_l(data, len);
}

const char *RtpSplitter::onSearchPacketTail_l(const char *data, int len) {
    //这是rtp包
    if (len < 2) {
        //数据不够
        return nullptr;
    }
    uint16_t length = (((uint8_t *) data)[0] << 8) | ((uint8_t *) data)[1];
    if (len < length + 2) {
        //数据不够
        return nullptr;
    }
    //返回rtp包末尾
    return data + 2 + length;
}

int64_t RtpSplitter::onRecvHeader(const char *data, uint64_t len) {
    char hikHeader[4] = {0x01,0x00,0x01,0x00};

    if (!strncmp(hikHeader, data, 4)) {
        data = data + 258;
        len = len - 258;
    }

    onRtpPacket(data,len);
    return 0;
}

}//namespace mediakit
#endif//defined(ENABLE_RTPPROXY)
