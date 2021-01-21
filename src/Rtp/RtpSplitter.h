/*
 * Copyright (c) 2016 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/xia-chu/ZLMediaKit).
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
    ~RtpSplitter() override;

protected:
    /**
     * 收到rtp包回调
     * @param data RTP包数据指针
     * @param len RTP包数据长度
     */
    virtual void onRtpPacket(const char *data, size_t len) = 0;

protected:
    ssize_t onRecvHeader(const char *data, size_t len) override;
    const char *onSearchPacketTail(const char *data, size_t len) override;
    const char *onSearchPacketTail_l(const char *data, size_t len);

private:
    bool _is_ehome = false;
    size_t _offset = 0;
};

}//namespace mediakit
#endif//defined(ENABLE_RTPPROXY)
#endif //ZLMEDIAKIT_RTPSPLITTER_H
