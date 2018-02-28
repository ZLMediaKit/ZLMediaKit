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

class HttpArgs : public StrCaseMap
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
    virtual void sendRequest(const string &url,float fTimeOutSec);
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
        _header = header;
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
    virtual void onRecv(const Buffer::Ptr &pBuf) override;
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
