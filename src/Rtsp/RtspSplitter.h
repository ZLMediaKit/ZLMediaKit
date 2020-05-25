/*
 * Copyright (c) 2016 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/xiongziliang/ZLMediaKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#ifndef ZLMEDIAKIT_RTSPSPLITTER_H
#define ZLMEDIAKIT_RTSPSPLITTER_H

#include "Common/Parser.h"
#include "Http/HttpRequestSplitter.h"

namespace mediakit{

class RtspSplitter : public HttpRequestSplitter{
public:
    RtspSplitter(){}
    virtual ~RtspSplitter(){}

    /**
    * 是否允许接收rtp包
    * @param enable
    */
    void enableRecvRtp(bool enable);
protected:
    /**
     * 收到完整的rtsp包回调，包括sdp等content数据
     * @param parser rtsp包
     */
    virtual void onWholeRtspPacket(Parser &parser) = 0;

    /**
     * 收到rtp包回调
     * @param data
     * @param len
     */
    virtual void onRtpPacket(const char *data,uint64_t len) = 0;

    /**
     * 从rtsp头中获取Content长度
     * @param parser
     * @return
     */
    virtual int64_t getContentLength(Parser &parser);
protected:
    const char *onSearchPacketTail(const char *data,int len) override ;
    const char *onSearchPacketTail_l(const char *data,int len) ;
    int64_t onRecvHeader(const char *data,uint64_t len) override;
    void onRecvContent(const char *data,uint64_t len) override;
private:
    bool _enableRecvRtp = false;
    bool _isRtpPacket = false;
    Parser _parser;
};

}//namespace mediakit



#endif //ZLMEDIAKIT_RTSPSPLITTER_H
