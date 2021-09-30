/*
 * Copyright (c) 2016 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/xia-chu/ZLMediaKit).
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

class HttpArgs : public map<string, variant, StrCaseCompare> {
public:
    HttpArgs() = default;
    ~HttpArgs() = default;

    string make() const {
        string ret;
        for (auto &pr : *this) {
            ret.append(pr.first);
            ret.append("=");
            ret.append(strCoding::UrlEncode(pr.second));
            ret.append("&");
        }
        if (ret.size()) {
            ret.pop_back();
        }
        return ret;
    }
};

class HttpClient : public TcpClient, public HttpRequestSplitter {
public:
    using HttpHeader = StrCaseMap;
    using Ptr = std::shared_ptr<HttpClient>;

    HttpClient() = default;
    ~HttpClient() override = default;

    /**
     * 发送http[s]请求
     * @param url 请求url
     * @param fTimeOutSec 超时时间
     */
    virtual void sendRequest(const string &url, float fTimeOutSec);

    /**
     * 重置对象
     */
    virtual void clear();

    /**
     * 设置http方法
     * @param method GET/POST等
     */
    void setMethod(string method);

    /**
     * 覆盖http头
     * @param header
     */
    void setHeader(HttpHeader header);

    HttpClient &addHeader(string key, string val, bool force = false);

    /**
     * 设置http content
     * @param body http content
     */
    void setBody(string body);

    /**
     * 设置http content
     * @param body http content
     */
    void setBody(HttpBody::Ptr body);

    /**
     * 获取回复，在收到完整回复后有效
     */
    const Parser &response() const;

    /**
     * 获取请求url
     */
    const string &getUrl() const;

protected:
    /**
     * 收到http回复头
     * @param status 状态码，譬如:200 OK
     * @param headers http头
     * @return 返回后续content的长度；-1:后续数据全是content；>=0:固定长度content
     *          需要指出的是，在http头中带有Content-Length字段时，该返回值无效
     */
    virtual ssize_t onResponseHeader(const string &status, const HttpHeader &headers) {
        //无Content-Length字段时默认后面全是content
        return -1;
    }

    /**
     * 收到http conten数据
     * @param buf 数据指针
     * @param size 数据大小
     * @param recvedSize 已收数据大小(包含本次数据大小),当其等于totalSize时将触发onResponseCompleted回调
     * @param totalSize 总数据大小
     */
    virtual void onResponseBody(const char *buf, size_t size, size_t recvedSize, size_t totalSize) {
        DebugL << size << " " << recvedSize << " " << totalSize;
    }

    /**
     * 接收http回复完毕,
     */
    virtual void onResponseCompleted() {
        DebugL;
    }

    /**
     * http链接断开回调
     * @param ex 断开原因
     */
    virtual void onDisconnect(const SockException &ex) {}

    /**
     * 重定向事件
     * @param url 重定向url
     * @param temporary 是否为临时重定向
     * @return 是否继续
     */
    virtual bool onRedirectUrl(const string &url, bool temporary) { return true; };

    //// HttpRequestSplitter override ////
    ssize_t onRecvHeader(const char *data, size_t len) override;
    void onRecvContent(const char *data, size_t len) override;

protected:
    //// TcpClient override ////
    void onConnect(const SockException &ex) override;
    void onRecv(const Buffer::Ptr &pBuf) override;
    void onErr(const SockException &ex) override;
    void onFlush() override;
    void onManager() override;

private:
    void onResponseCompleted_l();
    void checkCookie(HttpHeader &headers);

protected:
    bool _isHttps;

private:
    string _url;
    HttpHeader _header;
    HttpBody::Ptr _body;
    string _method;
    string _path;
    //recv
    size_t _recvedBodySize;
    ssize_t _totalBodySize;
    Parser _parser;
    string _lastHost;
    Ticker _aliveTicker;
    float _fTimeOutSec = 0;
    std::shared_ptr<HttpChunkedSplitter> _chunkedSplitter;
};

} /* namespace mediakit */

#endif /* Http_HttpClient_h */
