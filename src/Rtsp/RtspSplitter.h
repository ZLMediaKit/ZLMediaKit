/*
 * MIT License
 *
 * Copyright (c) 2016-2019 xiongziliang <771730766@qq.com>
 *
 * This file is part of ZLMediaKit(https://github.com/xiongziliang/ZLMediaKit).
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
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
    int64_t onRecvHeader(const char *data,uint64_t len) override;
    void onRecvContent(const char *data,uint64_t len) override;
private:
    bool _enableRecvRtp = false;
    bool _isRtpPacket = false;
    Parser _parser;
};

}//namespace mediakit



#endif //ZLMEDIAKIT_RTSPSPLITTER_H
