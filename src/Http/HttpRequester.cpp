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
