/*
 * Copyright (c) 2016-present The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/ZLMediaKit/ZLMediaKit).
 *
 * Use of this source code is governed by MIT-like license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#include <cstdlib>
#include "RtspSplitter.h"
#include "Util/util.h"
#include "Util/logger.h"
#include "Common/macros.h"

using namespace std;
using namespace toolkit;

namespace mediakit{

const char *RtspSplitter::onSearchPacketTail(const char *data, size_t len) {
    auto ret = onSearchPacketTail_l(data, len);
    if(ret){
        return ret;
    }

    if (len > 256 * 1024) {
        //rtp大于256KB
        ret = (char *) memchr(data, '$', len);
        if (!ret) {
            WarnL << "rtp缓存溢出:" << hexdump(data, 1024);
            reset();
        }
    }
    return ret;
}

const char *RtspSplitter::onSearchPacketTail_l(const char *data, size_t len) {
    if(!_enableRecvRtp || data[0] != '$'){
        //这是rtsp包
        _isRtpPacket = false;
        return HttpRequestSplitter::onSearchPacketTail(data, len);
    }
    //这是rtp包
    if(len < 4){
        //数据不够
        return nullptr;
    }
    uint16_t length = (((uint8_t *)data)[2] << 8) | ((uint8_t *)data)[3];
    if(len < (size_t)(length + 4)){
        //数据不够
        return nullptr;
    }
    //返回rtp包末尾
    _isRtpPacket = true;
    return data + 4 + length;
}

ssize_t RtspSplitter::onRecvHeader(const char *data, size_t len) {
    if (_isRtpPacket) {
        onRtpPacket(data, len);
        return 0;
    }
    if (len == 4 && !memcmp(data, "\r\n\r\n", 4)) {
        return 0;
    }
    try {
        _parser.parse(data, len);
    } catch (mediakit::AssertFailedException &ex){
        if (!_enableRecvRtp) {
            // 还在握手中，直接中断握手
            throw;
        }
        // 握手已经结束，如果rtsp server存在发送缓存溢出的bug，那么rtsp信令可能跟rtp混在一起
        // 这种情况下，rtsp信令解析异常不中断链接，只丢弃这个包
        WarnL << ex.what();
        return 0;
    }
    auto ret = getContentLength(_parser);
    if (ret == 0) {
        onWholeRtspPacket(_parser);
        _parser.clear();
    }
    return ret;
}

void RtspSplitter::onRecvContent(const char *data, size_t len) {
    _parser.setContent(string(data,len));
    onWholeRtspPacket(_parser);
    _parser.clear();
}

void RtspSplitter::enableRecvRtp(bool enable) {
    _enableRecvRtp = enable;
}

ssize_t RtspSplitter::getContentLength(Parser &parser) {
    return atoi(parser["Content-Length"].data());
}


}//namespace mediakit



