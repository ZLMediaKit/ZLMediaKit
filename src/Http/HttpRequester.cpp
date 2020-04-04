/*
 * Copyright (c) 2016 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/xiongziliang/ZLMediaKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#include "HttpRequester.h"

namespace mediakit{

HttpRequester::HttpRequester(){
    
}
HttpRequester::~HttpRequester(){
    
}

int64_t HttpRequester::onResponseHeader(const string &status,const HttpHeader &headers) {
    _strRecvBody.clear();
    //无Content-Length字段时默认后面没有content
    return 0;
}
    
void HttpRequester::onResponseBody(const char *buf,int64_t size,int64_t recvedSize,int64_t totalSize) {
    _strRecvBody.append(buf,size);
}
    
void HttpRequester::onResponseCompleted() {
    if(_onResult){
        _onResult(SockException(),responseStatus(),responseHeader(),_strRecvBody);
        _onResult = nullptr;
    }
}
    
void HttpRequester::onDisconnect(const SockException &ex){
    if(_onResult){
        const_cast<Parser &>(response()).setContent(_strRecvBody);
        _onResult(ex,responseStatus(),responseHeader(),_strRecvBody);
        _onResult = nullptr;
    }
}
    
void HttpRequester::startRequester(const string &url,const HttpRequesterResult &onResult , float timeOutSecond){
    _onResult = onResult;
    sendRequest(url,timeOutSecond);
}

void HttpRequester::clear() {
    HttpClientImp::clear();
    _strRecvBody.clear();
    _onResult = nullptr;
}

void HttpRequester::setOnResult(const HttpRequesterResult &onResult) {
    _onResult = onResult;
}


}//namespace mediakit
