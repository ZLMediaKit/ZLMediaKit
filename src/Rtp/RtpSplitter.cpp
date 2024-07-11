﻿/*
 * Copyright (c) 2016-present The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/ZLMediaKit/ZLMediaKit).
 *
 * Use of this source code is governed by MIT-like license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#if defined(ENABLE_RTPPROXY)
#include <string.h>
#include "RtpSplitter.h"
namespace mediakit{

static const int  kEHOME_OFFSET = 256;

ssize_t RtpSplitter::onRecvHeader(const char *data,size_t len){
    //忽略偏移量
    data += _offset;
    len -= _offset;

    if (_is_ehome && len > 12 && data[12] == '\r') {
        //这是ehome,移除第12个字节
        memmove((char *) data + 1, data, 12);
        data += 1;
        len -= 1;
    }
    onRtpPacket(data, len);
    return 0;
}

static bool isEhome(const char *data, size_t len){
    if (len < 4) {
        return false;
    }
    if ((data[0] == 0x01) && (data[1] == 0x00) && (data[2] >= 0x01)) {
        return true;
    }
    return false;
}

const char *RtpSplitter::onSearchPacketTail(const char *data, size_t len) {
    if (len < 4) {
        //数据不够
        return nullptr;
    }

    if ( _is_ehome ) {
        if (isEhome(data, len)) {
            //是ehome协议
            if (len < kEHOME_OFFSET + 4) {
                //数据不够
                return nullptr;
            }
            //忽略ehome私有头后是rtsp样式的rtp，多4个字节，
            _offset = kEHOME_OFFSET + 4;
            _is_ehome = true;
            //忽略ehome私有头
            return onSearchPacketTail_l(data + kEHOME_OFFSET + 2, len - kEHOME_OFFSET - 2);
        }
        _is_ehome = false;
    }

    if ( _is_rtsp_interleaved ) {
        if (data[0] == '$') {
            //可能是4个字节的rtp头
            _offset = 4;
            return onSearchPacketTail_l(data + 2, len - 2);
        }
        _is_rtsp_interleaved = false;
    }
    
    //两个字节的rtp头
    _offset = 2;
    return onSearchPacketTail_l(data, len);
}

const char *RtpSplitter::onSearchPacketTail_l(const char *data, size_t len) {
    //这是rtp包
    uint16_t length = (((uint8_t *) data)[0] << 8) | ((uint8_t *) data)[1];
    if (len < (size_t)(length + 2)) {
        //数据不够
        return nullptr;
    }
    //返回rtp包末尾
    return data + 2 + length;
}

}//namespace mediakit
#endif//defined(ENABLE_RTPPROXY)