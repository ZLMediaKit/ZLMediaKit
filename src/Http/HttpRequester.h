/*
 * Copyright (c) 2016 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/xiongziliang/ZLMediaKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#ifndef Htt_HttpRequester_h
#define Htt_HttpRequester_h

#include "HttpClientImp.h"

namespace mediakit{

class HttpRequester : public HttpClientImp
{
public:
    typedef std::shared_ptr<HttpRequester> Ptr;
    typedef std::function<void(const SockException &ex,const string &status,const HttpHeader &header,const string &strRecvBody)> HttpRequesterResult;
    HttpRequester();
    virtual ~HttpRequester();
    void setOnResult(const HttpRequesterResult &onResult);
    void startRequester(const string &url,const HttpRequesterResult &onResult,float timeOutSecond = 10);
    void clear() override ;
private:
    int64_t onResponseHeader(const string &status,const HttpHeader &headers) override;
    void onResponseBody(const char *buf,int64_t size,int64_t recvedSize,int64_t totalSize)  override;
    void onResponseCompleted() override;
    void onDisconnect(const SockException &ex) override;
private:
    string _strRecvBody;
    HttpRequesterResult _onResult;
};

}//namespace mediakit

#endif /* Htt_HttpRequester_h */
