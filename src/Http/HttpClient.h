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

#include <stdio.h>
#include <string.h>
#include <functional>
#include <memory>
#include "Rtsp/Rtsp.h"
#include "Util/util.h"
#include "Network/TcpClient.h"
#include "HttpRequestSplitter.h"
#include "HttpCookie.h"
#include "HttpChunkedSplitter.h"

using namespace std;
using namespace toolkit;

namespace mediakit {

class HttpArgs : public StrCaseMap {
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

class HttpBody{
public:
    typedef std::shared_ptr<HttpBody> Ptr;
    HttpBody(){}
    virtual ~HttpBody(){}
    //剩余数据大小
    virtual uint64_t remainSize() = 0;
    virtual Buffer::Ptr readData() = 0;
};

class HttpStringBody : public HttpBody{
public:
    typedef std::shared_ptr<HttpStringBody> Ptr;
    HttpStringBody(const string &str){
        _str = str;
    }
    virtual ~HttpStringBody(){}

    uint64_t remainSize() override {
        return _str.size();
    }
    Buffer::Ptr readData() override {
        auto ret = std::make_shared<BufferString>(_str);
        _str.clear();
        return ret;
    }
private:
    mutable string _str;
};


class HttpMultiFormBody : public HttpBody {
public:
    typedef std::shared_ptr<HttpMultiFormBody> Ptr;
    HttpMultiFormBody(const StrCaseMap &args,const string &filePath,const string &boundary,uint32_t sliceSize = 4 * 1024){
        _fp = fopen(filePath.data(),"rb");
        if(!_fp){
            throw std::invalid_argument(StrPrinter << "打开文件失败：" << filePath << " " << get_uv_errmsg());
        }
        auto fileName = filePath;
        auto pos = filePath.rfind('/');
        if(pos != string::npos){
            fileName = filePath.substr(pos + 1);
        }
        _bodyPrefix = multiFormBodyPrefix(args,boundary,fileName);
        _bodySuffix = multiFormBodySuffix(boundary);
        _totalSize =  _bodyPrefix.size() + _bodySuffix.size() + fileSize(_fp);
        _sliceSize = sliceSize;
    }
    virtual ~HttpMultiFormBody(){
        fclose(_fp);
    }

    uint64_t remainSize() override {
        return _totalSize - _offset;
    }

    Buffer::Ptr readData() override{
        if(_bodyPrefix.size()){
            auto ret = std::make_shared<BufferString>(_bodyPrefix);
            _offset += _bodyPrefix.size();
            _bodyPrefix.clear();
            return ret;
        }

        if(0 == feof(_fp)){
            auto ret = std::make_shared<BufferRaw>(_sliceSize);
            //读文件
            int size;
            do{
                size = fread(ret->data(),1,_sliceSize,_fp);
            }while(-1 == size && UV_EINTR == get_uv_error(false));

            if(size == -1){
                _offset = _totalSize;
                WarnL << "fread failed:" << get_uv_errmsg();
                return nullptr;
            }
            _offset += size;
            ret->setSize(size);
            return ret;
        }

        if(_bodySuffix.size()){
            auto ret = std::make_shared<BufferString>(_bodySuffix);
            _offset = _totalSize;
            _bodySuffix.clear();
            return ret;
        }

        return nullptr;
    }

public:
    static string multiFormBodyPrefix(const StrCaseMap &args,const string &boundary,const string &fileName){
        string MPboundary = string("--") + boundary;
        _StrPrinter body;
        for(auto &pr : args){
            body << MPboundary << "\r\n";
            body << "Content-Disposition: form-data; name=\"" << pr.first << "\"\r\n\r\n";
            body << pr.second << "\r\n";
        }
        body << MPboundary << "\r\n";
        body << "Content-Disposition: form-data; name=\"" << "file" << "\";filename=\"" << fileName << "\"\r\n";
        body << "Content-Type: application/octet-stream\r\n\r\n" ;
        return body;
    }
    static string multiFormBodySuffix(const string &boundary){
        string MPboundary = string("--") + boundary;
        string endMPboundary = MPboundary + "--";
        _StrPrinter body;
        body << "\r\n" << endMPboundary;
        return body;
    }

    static uint64_t fileSize(FILE *fp) {
        auto current = ftell(fp);
        fseek(fp,0L,SEEK_END); /* 定位到文件末尾 */
        auto end  = ftell(fp); /* 得到文件大小 */
        fseek(fp,current,SEEK_SET);
        return end - current;
    }

    static string multiFormContentType(const string &boundary){
        return StrPrinter << "multipart/form-data; boundary=" << boundary;
    }
private:
    FILE *_fp;
    string _bodyPrefix;
    string _bodySuffix;
    uint64_t _offset = 0;
    uint64_t _totalSize;
    uint32_t _sliceSize;
};



class HttpClient : public TcpClient , public HttpRequestSplitter
{
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
        _fTimeOutSec = 0;
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
        return _parser.getValues();
    }
    const Parser& response() const{
        return _parser;
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
     * 收到http回复数据回调
     * @param data 数据指针
     * @param size 数据大小
     */
    virtual void onRecvBytes(const char *data,int size);

    /**
     * http链接断开回调
     * @param ex 断开原因
     */
    virtual void onDisconnect(const SockException &ex){}

    //HttpRequestSplitter override
    int64_t onRecvHeader(const char *data,uint64_t len) override ;
    void onRecvContent(const char *data,uint64_t len) override;

    //在连接服务器前调用一次
    virtual void onBeforeConnect(string &strUrl, uint16_t &iPort,float &fTimeOutSec) {};
protected:
    virtual void onConnect(const SockException &ex) override;
    virtual void onRecv(const Buffer::Ptr &pBuf) override;
    virtual void onErr(const SockException &ex) override;
    virtual void onSend() override;
    virtual void onManager() override;
private:
    void onResponseCompleted_l();
    void checkCookie(const HttpHeader &headers );
protected:
    bool _isHttps;
private:
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
