//
//  HttpClient.h
//  ZLMediaKit
//
//  Created by xzl on 2017/5/4.
//

#ifndef Http_HttpClient_h
#define Http_HttpClient_h

#include <string.h>
#include <functional>
#include <memory>
#include "Rtsp/Rtsp.h"
#include "Util/util.h"
#include "Network/TcpClient.h"

using namespace std;
using namespace ZL::Util;
using namespace ZL::Network;

namespace ZL {
namespace Http {

class HttpArgs : public map<string,string>
{
public:
    HttpArgs(){}
    virtual ~HttpArgs(){}
    string make() const {
        string ret;
        for(auto &pr : *this){
            ret.append(pr.first);
            ret.append("=");
            ret.append(pr.second);
            ret.append("&");
        }
        if(ret.size()){
            ret.pop_back();
        }
        return ret;
    }
};
    
class HttpClient : public TcpClient
{
public:
    typedef StrCaseMap HttpHeader;
    typedef std::shared_ptr<HttpClient> Ptr;
    HttpClient();
    virtual ~HttpClient();
    virtual void sendRequest(const string &url);
    void clear(){
        _header.clear();
        _body.clear();
        _method.clear();
        _path.clear();
        _recvedResponse.clear();
        _parser.Clear();
    }
    void setMethod(const string &method){
        _method = method;
    }
    void setHeader(const HttpHeader &header){
        _header = _header;
    }
    void addHeader(const string &key,const string &val){
        _header.emplace(key,val);
    }
    void setBody(const string &body){
        _body = body;
    }
    const string &responseStatus(){
        return _parser.Url();
    }
    const HttpHeader &responseHeader(){
        return _parser.getValues();
    }
protected:
    bool _isHttps;
    
    virtual void onResponseHeader(const string &status,const HttpHeader &headers){
        DebugL << status;
    };
    virtual void onResponseBody(const char *buf,size_t size,size_t recvedSize,size_t totalSize){
        DebugL << size << " " <<  recvedSize << " " << totalSize;
    };
    virtual void onResponseCompleted(){
    	DebugL;
    }
    virtual void onRecvBytes(const char *data,int size);
    virtual void onDisconnect(const SockException &ex){}
private:
    virtual void onConnect(const SockException &ex) override;
    virtual void onRecv(const Socket::Buffer::Ptr &pBuf) override;
    virtual void onErr(const SockException &ex) override;
    
    //send
    HttpHeader _header;
    string _body;
    string _method;
    string _path;
    
    //recv
    string _recvedResponse;
    size_t _recvedBodySize;
    size_t _totalBodySize;
    Parser _parser;
    
    string _lastHost;
};








} /* namespace Http */
} /* namespace ZL */








#endif /* Http_HttpClient_h */
