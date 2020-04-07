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
namespace mediakit{

RtpSplitter::RtpSplitter() {
}

RtpSplitter::~RtpSplitter() {
}

const char *RtpSplitter::onSearchPacketTail(const char *data, int len) {
    //这是rtp包
    if(len < 2){
        //数据不够
        return nullptr;
    }
    uint16_t length = (((uint8_t *)data)[0] << 8) | ((uint8_t *)data)[1];
    if(len < length + 2){
        //数据不够
        return nullptr;
    }
    //返回rtp包末尾
    return data + 2 + length;
}

int64_t RtpSplitter::onRecvHeader(const char *data, uint64_t len) {
    onRtpPacket(data,len);
    return 0;
}

}//namespace mediakit
#endif//defined(ENABLE_RTPPROXY)