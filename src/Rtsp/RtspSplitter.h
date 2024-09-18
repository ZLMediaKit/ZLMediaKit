/*
 * Copyright (c) 2016-present The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/ZLMediaKit/ZLMediaKit).
 *
 * Use of this source code is governed by MIT-like license that can be found in the
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
    /**
    * 是否允许接收rtp包
    * @param enable
     * Whether to allow receiving rtp packets
     * @param enable
     
     * [AUTO-TRANSLATED:8de8e1ee]
    */
    void enableRecvRtp(bool enable);
protected:
    /**
     * 收到完整的rtsp包回调，包括sdp等content数据
     * @param parser rtsp包
     * Callback for receiving a complete rtsp packet, including sdp and other content data
     * @param parser rtsp packet
     
     * [AUTO-TRANSLATED:4d3c2056]
     */
    virtual void onWholeRtspPacket(Parser &parser) = 0;

    /**
     * 收到rtp包回调
     * @param data
     * @param len
     * Callback for receiving rtp packets
     * @param data
     * @param len
     
     * [AUTO-TRANSLATED:c8f7c9bb]
     */
    virtual void onRtpPacket(const char *data,size_t len) = 0;

    /**
     * 从rtsp头中获取Content长度
     * @param parser
     * @return
     * Get the Content length from the rtsp header
     * @param parser
     * @return
     
     
     * [AUTO-TRANSLATED:f0bc1fb8]
     */
    virtual ssize_t getContentLength(Parser &parser);

protected:
    const char *onSearchPacketTail(const char *data,size_t len) override ;
    const char *onSearchPacketTail_l(const char *data,size_t len) ;
    ssize_t onRecvHeader(const char *data,size_t len) override;
    void onRecvContent(const char *data,size_t len) override;

private:
    bool _enableRecvRtp = false;
    bool _isRtpPacket = false;
    Parser _parser;
};

}//namespace mediakit



#endif //ZLMEDIAKIT_RTSPSPLITTER_H
