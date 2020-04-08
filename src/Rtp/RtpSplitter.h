/*
 * Copyright (c) 2016 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/xiongziliang/ZLMediaKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#ifndef ZLMEDIAKIT_RTPSPLITTER_H
#define ZLMEDIAKIT_RTPSPLITTER_H

#if defined(ENABLE_RTPPROXY)
#include "Http/HttpRequestSplitter.h"

namespace mediakit{

class RtpSplitter : public HttpRequestSplitter{
public:
    RtpSplitter();
    virtual ~RtpSplitter();
protected:
    /**
     * 收到rtp包回调
     * @param data
     * @param len
     */
    virtual void onRtpPacket(const char *data,uint64_t len) = 0;
protected:
    const char *onSearchPacketTail(const char *data,int len) override ;
    int64_t onRecvHeader(const char *data,uint64_t len) override;
};

}//namespace mediakit
#endif//defined(ENABLE_RTPPROXY)
#endif //ZLMEDIAKIT_RTPSPLITTER_H
