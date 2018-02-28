/*
 * MIT License
 *
 * Copyright (c) 2016 xiongziliang <771730766@qq.com>
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

#ifndef Htt_HttpRequester_h
#define Htt_HttpRequester_h

#include "HttpClientImp.h"

namespace ZL{
namespace Http{
    
class HttpRequester : public HttpClientImp
{
public:
    typedef std::shared_ptr<HttpRequester> Ptr;
    typedef std::function<void(const SockException &ex,const string &status,const HttpHeader &header,const string &strRecvBody)> HttpRequesterResult;
    HttpRequester();
    virtual ~HttpRequester();
    
    void startRequester(const string &url,const HttpRequesterResult &onResult,float timeOutSecond = 10);
private:
    void onResponseHeader(const string &status,const HttpHeader &headers) override;
    void onResponseBody(const char *buf,size_t size,size_t recvedSize,size_t totalSize)  override;
    void onResponseCompleted() override;
    void onDisconnect(const SockException &ex) override;
    void onManager() override;
    string _strRecvBody;
    HttpRequesterResult _onResult;
    Ticker _resTicker;
    float _timeOutSecond;
};

}//namespace Http
}//namespace ZL

#endif /* Htt_HttpRequester_h */
