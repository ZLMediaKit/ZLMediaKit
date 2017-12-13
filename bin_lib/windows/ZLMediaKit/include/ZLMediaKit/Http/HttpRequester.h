//
//  HttpRequester_h
//  ZLMediaKit
//
//  Created by xzl on 2017/5/5.
//

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
    
    void startRequester(const string &url,const HttpRequesterResult &onResult);
private:
    void onResponseHeader(const string &status,const HttpHeader &headers) override;
    void onResponseBody(const char *buf,size_t size,size_t recvedSize,size_t totalSize)  override;
    void onResponseCompleted() override;
    void onDisconnect(const SockException &ex) override;
    
    string _strRecvBody;
    HttpRequesterResult _onResult;
    
    
};

}//namespace Http
}//namespace ZL

#endif /* Htt_HttpRequester_h */
