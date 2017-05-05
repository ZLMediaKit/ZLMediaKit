//
//  HttpRequester.cpp
//  ZLMediaKit
//
//  Created by xzl on 2017/5/5.
//

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
    
void HttpRequester::startRequester(const string &url,const HttpRequesterResult &onResult){
    _onResult = onResult;
    sendRequest(url);
    
}


}//namespace Http
}//namespace ZL
