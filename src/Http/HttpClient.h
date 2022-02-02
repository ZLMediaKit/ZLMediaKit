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

namespace mediakit {

class HttpArgs : public std::map<std::string, toolkit::variant, StrCaseCompare> {
public:
    HttpArgs() = default;
    ~HttpArgs() = default;

    std::string make() const {
        std::string ret;
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

class HttpClient : public toolkit::TcpClient, public HttpRequestSplitter {
public:
    using HttpHeader = StrCaseMap;
    using Ptr = std::shared_ptr<HttpClient>;

    HttpClient() = default;
    ~HttpClient() override = default;

    /**
     * 发送http[s]请求
     * @param url 请求url
     */
    virtual void sendRequest(const std::string &url);

    /**
     * 重置对象
     */
    virtual void clear();

    /**
     * 设置http方法
     * @param method GET/POST等
     */
    void setMethod(std::string method);

    /**
     * 覆盖http头
     * @param header
     */
    void setHeader(HttpHeader header);

    HttpClient &addHeader(std::string key, std::string val, bool force = false);

    /**
     * 设置http content
     * @param body http content
     */
    void setBody(std::string body);

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
     * 获取回复header声明的body大小
     */
    ssize_t responseBodyTotalSize() const;

    /**
     * 获取已经下载body的大小
     */
    size_t responseBodySize() const;

    /**
     * 获取请求url
     */
    const std::string &getUrl() const;

    /**
     * 判断是否正在等待响应
     */
    bool waitResponse() const;

    /**
     * 判断是否为https
     */
    bool isHttps() const;

    /**
     * 设置从发起连接到接收header完毕的延时，默认10秒
     * 此参数必须大于0
     */
    void setHeaderTimeout(size_t timeout_ms);

    /**
     * 设置接收body数据超时时间, 默认5秒
     * 此参数可以用于处理超大body回复的超时问题
     * 此参数可以等于0
     */
    void setBodyTimeout(size_t timeout_ms);

    /**
     * 设置整个链接超时超时时间, 默认0
     * 该值设置不为0后，HeaderTimeout和BodyTimeout无效
     */
    void setCompleteTimeout(size_t timeout_ms);

protected:
    /**
     * 收到http回复头
     * @param status 状态码，譬如:200 OK
     * @param headers http头
     */
    virtual void onResponseHeader(const std::string &status, const HttpHeader &headers) = 0;

    /**
     * 收到http conten数据
     * @param buf 数据指针
     * @param size 数据大小
     */
    virtual void onResponseBody(const char *buf, size_t size) = 0;

    /**
     * 接收http回复完毕,
     */
    virtual void onResponseCompleted(const toolkit::SockException &ex) = 0;

    /**
     * 重定向事件
     * @param url 重定向url
     * @param temporary 是否为临时重定向
     * @return 是否继续
     */
    virtual bool onRedirectUrl(const std::string &url, bool temporary) { return true; };

protected:
    //// HttpRequestSplitter override ////
    ssize_t onRecvHeader(const char *data, size_t len) override;
    void onRecvContent(const char *data, size_t len) override;

    //// TcpClient override ////
    void onConnect(const toolkit::SockException &ex) override;
    void onRecv(const toolkit::Buffer::Ptr &pBuf) override;
    void onErr(const toolkit::SockException &ex) override;
    void onFlush() override;
    void onManager() override;

private:
    void onResponseCompleted_l(const toolkit::SockException &ex);
    void onConnect_l(const toolkit::SockException &ex);
    void checkCookie(HttpHeader &headers);
    void clearResponse();

private:
    //for http response
    bool _complete = false;
    bool _header_recved = false;
    size_t _recved_body_size;
    ssize_t _total_body_size;
    Parser _parser;
    std::shared_ptr<HttpChunkedSplitter> _chunked_splitter;

    //for request args
    bool _is_https;
    std::string _url;
    HttpHeader _user_set_header;
    HttpBody::Ptr _body;
    std::string _method;
    std::string _last_host;

    //for this request
    std::string _path;
    HttpHeader _header;

    //for timeout
    size_t _wait_header_ms = 10 * 1000;
    size_t _wait_body_ms = 10 * 1000;
    size_t _wait_complete_ms = 0;
    toolkit::Ticker _wait_header;
    toolkit::Ticker _wait_body;
    toolkit::Ticker _wait_complete;
};

} /* namespace mediakit */

#endif /* Http_HttpClient_h */
