/*
 * Copyright (c) 2016-present The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/ZLMediaKit/ZLMediaKit).
 *
 * Use of this source code is governed by MIT-like license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#ifndef ZLMEDIAKIT_FLVSPLITTER_H
#define ZLMEDIAKIT_FLVSPLITTER_H

#include "Rtmp.h"
#include "Http/HttpRequestSplitter.h"
#include "RtmpPlayerImp.h"

namespace mediakit {

class FlvSplitter : public HttpRequestSplitter {
protected:
    void onRecvContent(const char *data,size_t len) override;
    ssize_t onRecvHeader(const char *data,size_t len) override;
    const char *onSearchPacketTail(const char *data, size_t len) override;

protected:
    virtual void onRecvFlvHeader(const FLVHeader &header) {};
    virtual bool onRecvMetadata(const AMFValue &metadata) = 0;
    virtual void onRecvRtmpPacket(RtmpPacket::Ptr packet) = 0;

private:
    bool _flv_started = false;
    uint8_t _type;
    uint32_t _time_stamp;
};

}//namespace mediakit
#endif //ZLMEDIAKIT_FLVSPLITTER_H
