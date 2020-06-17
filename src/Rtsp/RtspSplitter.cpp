/*
 * Copyright (c) 2016 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/xiongziliang/ZLMediaKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#include <cstdlib>
#include "RtspSplitter.h"
#include "Util/logger.h"
#include "Util/util.h"

namespace mediakit{

const char *RtspSplitter::onSearchPacketTail(const char *data, int len) {
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

const char *RtspSplitter::onSearchPacketTail_l(const char *data, int len) {
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
    if(len < length + 4){
        //数据不够
        return nullptr;
    }
    //返回rtp包末尾
    _isRtpPacket = true;
    return data + 4 + length;
}

int64_t RtspSplitter::onRecvHeader(const char *data, uint64_t len) {
    if(_isRtpPacket){
        onRtpPacket(data,len);
        return 0;
    }
    _parser.Parse(data);
    auto ret = getContentLength(_parser);
    if(ret == 0){
        onWholeRtspPacket(_parser);
        _parser.Clear();
    }
    return ret;
}

void RtspSplitter::onRecvContent(const char *data, uint64_t len) {
    _parser.setContent(string(data,len));
    onWholeRtspPacket(_parser);
    _parser.Clear();
}

void RtspSplitter::enableRecvRtp(bool enable) {
    _enableRecvRtp = enable;
}

int64_t RtspSplitter::getContentLength(Parser &parser) {
    return atoi(parser["Content-Length"].data());
}


}//namespace mediakit



