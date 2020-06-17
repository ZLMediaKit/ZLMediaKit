/*
 * Copyright (c) 2016 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/xiongziliang/ZLMediaKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#ifndef Http_HttpClient_h
#define Http_HttpClient_h

#include <stdio.h>
#include <string.h>
#include <functional>
#include <memory>
#include "Util/util.h"
#include "Util/mini.h"
#include "Network/TcpClient.h"
#include "Common/Parser.h"
#include "HttpRequestSplitter.h"
#include "HttpCookie.h"
#include "HttpChunkedSplitter.h"
#include "strCoding.h"
#include "HttpBody.h"
using namespace std;
using namespace toolkit;

namespace mediakit {

class HttpArgs : public map<string, variant, StrCaseCompare>  {
public:
    HttpArgs(){}
    virtual ~HttpArgs(){}
    string make() const {
        string ret;
        for(auto &pr : *this){
            ret.append(pr.first);
            ret.append("=");
            ret.append(strCoding::UrlEncode(pr.second));
            ret.append("&");
        }
        if(ret.size()){
            ret.pop_back();
        }
        return ret;
    }
};

class HttpClient : public TcpClient , public HttpRequestSplitter{
public:
    typedef StrCaseMap HttpHeader;
    typedef std::shared_ptr<HttpClient> Ptr;
    HttpClient();
    virtual ~HttpClient();
    virtual void sendRequest(const string &url,float fTimeOutSec);

    virtual void clear(){
        _header.clear();
        _body.reset();
        _method.clear();
        _path.clear();
        _parser.Clear();
        _recvedBodySize = 0;
        _totalBodySize = 0;
        _aliveTicker.resetTime();
        _chunkedSplitter.reset();
        HttpRequestSplitter::reset();
    }

    void setMethod(const string &method){
        _method = method;
    }
    void setHeader(const HttpHeader &header){
        _header = header;
    }
    HttpClient & addHeader(const string &key,const string &val,bool force = false){
        if(!force){
            _header.emplace(key,val);
        }else{
            _header[key] = val;
        }
        return *this;
    }
    void setBody(const string &body){
        _body.reset(new HttpStringBody(body));
    }
    void setBody(const HttpBody::Ptr &body){
        _body = body;
    }
    const string &responseStatus() const{
        return _parser.Url();
    }
    const HttpHeader &responseHeader() const{
        return _parser.getHeader();
    }
    const Parser& response() const{
        return _parser;
    }

    const string &getUrl() const{
        return _url;
    }
protected:
    /**
     * 收到http回复头
     * @param status 状态码，譬如:200 OK
     * @param headers http头
     * @return 返回后续content的长度；-1:后续数据全是content；>=0:固定长度content
     *          需要指出的是，在http头中带有Content-Length字段时，该返回值无效
     */
    virtual int64_t onResponseHeader(const string &status,const HttpHeader &headers){
        DebugL << status;
        //无Content-Length字段时默认后面全是content
        return -1;
    };

    /**
     * 收到http conten数据
     * @param buf 数据指针
     * @param size 数据大小
     * @param recvedSize 已收数据大小(包含本次数据大小),当其等于totalSize时将触发onResponseCompleted回调
     * @param totalSize 总数据大小
     */
    virtual void onResponseBody(const char *buf,int64_t size,int64_t recvedSize,int64_t totalSize){
        DebugL << size << " " <<  recvedSize << " " << totalSize;
    };

    /**
     * 接收http回复完毕,
     */
    virtual void onResponseCompleted(){
        DebugL;
    }

    /**
     * http链接断开回调
     * @param ex 断开原因
     */
    virtual void onDisconnect(const SockException &ex){}

    /**
     * 重定向事件
     * @param url 重定向url
     * @param temporary 是否为临时重定向
     * @return 是否继续
     */
    virtual bool onRedirectUrl(const string &url,bool temporary){ return true;};

    //HttpRequestSplitter override
    int64_t onRecvHeader(const char *data,uint64_t len) override ;
    void onRecvContent(const char *data,uint64_t len) override;
protected:
    virtual void onConnect(const SockException &ex) override;
    virtual void onRecv(const Buffer::Ptr &pBuf) override;
    virtual void onErr(const SockException &ex) override;
    virtual void onFlush() override;
    virtual void onManager() override;
private:
    void onResponseCompleted_l();
    void checkCookie(HttpHeader &headers );
protected:
    bool _isHttps;
private:
    string _url;
    HttpHeader _header;
    HttpBody::Ptr _body;
    string _method;
    string _path;
    //recv
    int64_t _recvedBodySize;
    int64_t _totalBodySize;
    Parser _parser;
    string _lastHost;
    Ticker _aliveTicker;
    float _fTimeOutSec = 0;
    std::shared_ptr<HttpChunkedSplitter> _chunkedSplitter;
};

} /* namespace mediakit */

#endif /* Http_HttpClient_h */
