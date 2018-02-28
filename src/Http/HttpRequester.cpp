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
#include "HttpRequester.h"

namespace ZL{
namespace Http{

HttpRequester::HttpRequester(){
    
}
HttpRequester::~HttpRequester(){
    
}

void HttpRequester::onResponseHeader(const string &status,const HttpHeader &headers) {
    _strRecvBody.clear();
}
    
void HttpRequester::onResponseBody(const char *buf,size_t size,size_t recvedSize,size_t totalSize) {
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
        _onResult(ex,responseStatus(),responseHeader(),_strRecvBody);
        _onResult = nullptr;
    }
}
    
void HttpRequester::startRequester(const string &url,const HttpRequesterResult &onResult , float timeOutSecond){
    _onResult = onResult;
    _resTicker.resetTime();
    _timeOutSecond = timeOutSecond;
    sendRequest(url,timeOutSecond);
}

void HttpRequester::onManager(){
    if(_onResult && _resTicker.elapsedTime() > _timeOutSecond * 1000){
        //超时
        onDisconnect(SockException(Err_timeout,"wait http response timeout"));
        shutdown();
    }
}


}//namespace Http
}//namespace ZL
