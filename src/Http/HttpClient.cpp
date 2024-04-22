/*
 * Copyright (c) 2016-present The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/ZLMediaKit/ZLMediaKit).
 *
 * Use of this source code is governed by MIT-like license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#include <cstdlib>
#include "Util/base64.h"
#include "HttpClient.h"
#include "Common/config.h"

using namespace std;
using namespace toolkit;

namespace mediakit {

void HttpClient::sendRequest(const string &url) {
    clearResponse();
    _url = url;
    auto protocol = findSubString(url.data(), NULL, "://");
    uint16_t port;
    bool is_https;
    if (strcasecmp(protocol.data(), "http") == 0) {
        port = 80;
        is_https = false;
    } else if (strcasecmp(protocol.data(), "https") == 0) {
        port = 443;
        is_https = true;
    } else {
        auto strErr = StrPrinter << "非法的http url:" << url << endl;
        throw std::invalid_argument(strErr);
    }

    auto host = findSubString(url.data(), "://", "/");
    if (host.empty()) {
        host = findSubString(url.data(), "://", NULL);
    }
    _path = findSubString(url.data(), host.data(), NULL);
    if (_path.empty()) {
        _path = "/";
    }
    //重新设置header，防止上次请求的header干扰
    _header = _user_set_header;
    auto pos = host.find('@');
    if (pos != string::npos) {
        //去除？后面的字符串
        auto authStr = host.substr(0, pos);
        host = host.substr(pos + 1, host.size());
        _header.emplace("Authorization", "Basic " + encodeBase64(authStr));
    }
    auto host_header = host;
    splitUrl(host, host, port);
    _header.emplace("Host", host_header);
    _header.emplace("User-Agent", kServerName);
    _header.emplace("Accept", "*/*");
    _header.emplace("Accept-Language", "zh-CN,zh;q=0.8");
    if (_http_persistent) {
        _header.emplace("Connection", "keep-alive");
    } else {
        _header.emplace("Connection", "close");
    }
    _http_persistent = true;
    if (_body && _body->remainSize()) {
        _header.emplace("Content-Length", to_string(_body->remainSize()));
        GET_CONFIG(string, charSet, Http::kCharSet);
        _header.emplace("Content-Type", "application/x-www-form-urlencoded; charset=" + charSet);
    }

    bool host_changed = (_last_host != host + ":" + to_string(port)) || (_is_https != is_https);
    _last_host = host + ":" + to_string(port);
    _is_https = is_https;

    auto cookies = HttpCookieStorage::Instance().get(_last_host, _path);
    _StrPrinter printer;
    for (auto &cookie : cookies) {
        printer << cookie->getKey() << "=" << cookie->getVal() << ";";
    }
    if (!printer.empty()) {
        printer.pop_back();
        _header.emplace("Cookie", printer);
    }
    if (!alive() || host_changed || !_http_persistent) {
        if (isUsedProxy()) {
            _proxy_connected = false;
            startConnect(_proxy_host, _proxy_port, _wait_header_ms / 1000.0f);
        } else {
            startConnect(host, port, _wait_header_ms / 1000.0f);
        }
    } else {
        SockException ex;
        onConnect_l(ex);
    }
}

void HttpClient::clear() {
    _url.clear();
    _user_set_header.clear();
    _body.reset();
    _method.clear();
    clearResponse();
}

void HttpClient::clearResponse() {
    _complete = false;
    _header_recved = false;
    _recved_body_size = 0;
    _total_body_size = 0;
    _parser.clear();
    _chunked_splitter = nullptr;
    _wait_header.resetTime();
    _wait_body.resetTime();
    _wait_complete.resetTime();
    HttpRequestSplitter::reset();
}

void HttpClient::setMethod(string method) {
    _method = std::move(method);
}

void HttpClient::setHeader(HttpHeader header) {
    _user_set_header = std::move(header);
}

HttpClient &HttpClient::addHeader(string key, string val, bool force) {
    if (!force) {
        _user_set_header.emplace(std::move(key), std::move(val));
    } else {
        _user_set_header[std::move(key)] = std::move(val);
    }
    return *this;
}

void HttpClient::setBody(string body) {
    _body.reset(new HttpStringBody(std::move(body)));
}

void HttpClient::setBody(HttpBody::Ptr body) {
    _body = std::move(body);
}

const Parser &HttpClient::response() const {
    return _parser;
}

ssize_t HttpClient::responseBodyTotalSize() const {
    return _total_body_size;
}

size_t HttpClient::responseBodySize() const {
    return _recved_body_size;
}

const string &HttpClient::getUrl() const {
    return _url;
}

void HttpClient::onConnect(const SockException &ex) {
    onConnect_l(ex);
}

void HttpClient::onConnect_l(const SockException &ex) {
    if (ex) {
        onResponseCompleted_l(ex);
        return;
    }
    _StrPrinter printer;
    //不使用代理或者代理服务器已经连接成功
    if (_proxy_connected || !isUsedProxy()) {
        printer << _method + " " << _path + " HTTP/1.1\r\n";
        for (auto &pr : _header) {
            printer << pr.first + ": ";
            printer << pr.second + "\r\n";
        }
        _header.clear();
        _path.clear();
    } else {
        printer << "CONNECT " << _last_host << " HTTP/1.1\r\n";
        printer << "Proxy-Connection: keep-alive\r\n";
        if (!_proxy_auth.empty()) {
            printer << "Proxy-Authorization: Basic " << _proxy_auth << "\r\n";
        }
    }
    SockSender::send(printer << "\r\n");
    onFlush();
}

void HttpClient::onRecv(const Buffer::Ptr &pBuf) {
    _wait_body.resetTime();
    HttpRequestSplitter::input(pBuf->data(), pBuf->size());
}

void HttpClient::onError(const SockException &ex) {
    if (ex.getErrCode() == Err_reset && _allow_resend_request && _http_persistent && _recved_body_size == 0 && !_header_recved) {
        // 连接被重置，可能是服务器主动断开了连接, 或者服务器内核参数或防火墙的持久连接空闲时间超时或不一致.
        // 如果是持久化连接，那么我们可以通过重连来解决这个问题
        // The connection was reset, possibly because the server actively disconnected the connection,
        // or the persistent connection idle time of the server kernel parameters or firewall timed out or inconsistent.
        // If it is a persistent connection, then we can solve this problem by reconnecting
        WarnL << "http persistent connect reset, try reconnect";
        _http_persistent = false;
        sendRequest(_url);
        return;
    }
    onResponseCompleted_l(ex);
}

ssize_t HttpClient::onRecvHeader(const char *data, size_t len) {
    _parser.parse(data, len);
    if (_parser.status() == "302" || _parser.status() == "301" || _parser.status() == "303") {
        auto new_url = Parser::mergeUrl(_url, _parser["Location"]);
        if (new_url.empty()) {
            throw invalid_argument("未找到Location字段(跳转url)");
        }
        if (onRedirectUrl(new_url, _parser.status() == "302")) {
            HttpClient::sendRequest(new_url);
            return 0;
        }
    }

    checkCookie(_parser.getHeader());
    onResponseHeader(_parser.status(), _parser.getHeader());
    _header_recved = true;

    if (_parser["Transfer-Encoding"] == "chunked") {
        //如果Transfer-Encoding字段等于chunked，则认为后续的content是不限制长度的
        _total_body_size = -1;
        _chunked_splitter = std::make_shared<HttpChunkedSplitter>([this](const char *data, size_t len) {
            if (len > 0) {
                _recved_body_size += len;
                onResponseBody(data, len);
            } else {
                _total_body_size = _recved_body_size;
                if (_recved_body_size > 0) {
                    onResponseCompleted_l(SockException(Err_success, "success"));
                } else {
                    onResponseCompleted_l(SockException(Err_other, "no body"));
                }
            }
        });
        //后续为源源不断的body
        return -1;
    }

    if (!_parser["Content-Length"].empty()) {
        //有Content-Length字段时忽略onResponseHeader的返回值
        _total_body_size = atoll(_parser["Content-Length"].data());
    } else {
        _total_body_size = -1;
    }

    if (_total_body_size == 0) {
        //后续没content，本次http请求结束
        onResponseCompleted_l(SockException(Err_success, "The request is successful but has no body"));
        return 0;
    }

    //当_total_body_size != 0时到达这里，代表后续有content
    //虽然我们在_total_body_size >0 时知道content的确切大小，
    //但是由于我们没必要等content接收完毕才回调onRecvContent(因为这样浪费内存并且要多次拷贝数据)
    //所以返回-1代表我们接下来分段接收content
    _recved_body_size = 0;
    return -1;
}

void HttpClient::onRecvContent(const char *data, size_t len) {
    if (_chunked_splitter) {
        _chunked_splitter->input(data, len);
        return;
    }
    _recved_body_size += len;
    if (_total_body_size < 0) {
        //不限长度的content
        onResponseBody(data, len);
        return;
    }

    //固定长度的content
    if (_recved_body_size < (size_t) _total_body_size) {
        //content还未接收完毕
        onResponseBody(data, len);
        return;
    }

    if (_recved_body_size == (size_t)_total_body_size) {
        //content接收完毕
        onResponseBody(data, len);
        onResponseCompleted_l(SockException(Err_success, "completed"));
        return;
    }

    //声明的content数据比真实的小，断开链接
    onResponseBody(data, len);
    throw invalid_argument("http response content size bigger than expected");
}

void HttpClient::onFlush() {
    GET_CONFIG(uint32_t, send_buf_size, Http::kSendBufSize);
    while (_body && _body->remainSize() && !isSocketBusy()) {
        auto buffer = _body->readData(send_buf_size);
        if (!buffer) {
            //数据发送结束或读取数据异常
            break;
        }
        if (send(buffer) <= 0) {
            //发送数据失败，不需要回滚数据，因为发送前已经通过isSocketBusy()判断socket可写
            //所以发送缓存区肯定未满,该buffer肯定已经写入socket
            break;
        }
    }
}

void HttpClient::onManager() {
    //onManager回调在连接中或已连接状态才会调用

    if (_wait_complete_ms > 0) {
        //设置了总超时时间
        if (!_complete && _wait_complete.elapsedTime() > _wait_complete_ms) {
            //等待http回复完毕超时
            shutdown(SockException(Err_timeout, "wait http response complete timeout"));
            return;
        }
        return;
    }

    //未设置总超时时间
    if (!_header_recved) {
        //等待header中
        if (_wait_header.elapsedTime() > _wait_header_ms) {
            //等待header中超时
            shutdown(SockException(Err_timeout, "wait http response header timeout"));
            return;
        }
    } else if (_wait_body_ms > 0 && _wait_body.elapsedTime() > _wait_body_ms) {
        //等待body中，等待超时
        shutdown(SockException(Err_timeout, "wait http response body timeout"));
        return;
    }
}

void HttpClient::onResponseCompleted_l(const SockException &ex) {
    if (_complete) {
        return;
    }
    _complete = true;
    _wait_complete.resetTime();

    if (!ex) {
        //确认无疑的成功
        onResponseCompleted(ex);
        return;
    }
    //可疑的失败

    if (_total_body_size > 0 && _recved_body_size >= (size_t)_total_body_size) {
        //回复header中有content-length信息，那么收到的body大于等于声明值则认为成功
        onResponseCompleted(SockException(Err_success, "read body completed"));
        return;
    }

    if (_total_body_size == -1 && _recved_body_size > 0) {
        //回复header中无content-length信息，那么收到一点body也认为成功
        onResponseCompleted(SockException(Err_success, ex.what()));
        return;
    }

    //确认无疑的失败
    onResponseCompleted(ex);
}

bool HttpClient::waitResponse() const {
    return !_complete && alive();
}

bool HttpClient::isHttps() const {
    return _is_https;
}

void HttpClient::checkCookie(HttpClient::HttpHeader &headers) {
    //Set-Cookie: IPTV_SERVER=8E03927B-CC8C-4389-BC00-31DBA7EC7B49;expires=Sun, Sep 23 2018 15:07:31 GMT;path=/index/api/
    for (auto it_set_cookie = headers.find("Set-Cookie"); it_set_cookie != headers.end(); ++it_set_cookie) {
        auto key_val = Parser::parseArgs(it_set_cookie->second, ";", "=");
        HttpCookie::Ptr cookie = std::make_shared<HttpCookie>();
        cookie->setHost(_last_host);

        int index = 0;
        auto arg_vec = split(it_set_cookie->second, ";");
        for (string &key_val : arg_vec) {
            auto key = findSubString(key_val.data(), NULL, "=");
            auto val = findSubString(key_val.data(), "=", NULL);

            if (index++ == 0) {
                cookie->setKeyVal(key, val);
                continue;
            }

            if (key == "path") {
                cookie->setPath(val);
                continue;
            }

            if (key == "expires") {
                cookie->setExpires(val, headers["Date"]);
                continue;
            }
        }

        if (!(*cookie)) {
            //无效的cookie
            continue;
        }
        HttpCookieStorage::Instance().set(cookie);
    }
}

void HttpClient::setHeaderTimeout(size_t timeout_ms) {
    CHECK(timeout_ms > 0);
    _wait_header_ms = timeout_ms;
}

void HttpClient::setBodyTimeout(size_t timeout_ms) {
    _wait_body_ms = timeout_ms;
}

void HttpClient::setCompleteTimeout(size_t timeout_ms) {
    _wait_complete_ms = timeout_ms;
}

bool HttpClient::isUsedProxy() const {
    return _used_proxy;
}

bool HttpClient::isProxyConnected() const {
    return _proxy_connected;
}

void HttpClient::setProxyUrl(string proxy_url) {
    _proxy_url = std::move(proxy_url);
    if (!_proxy_url.empty()) {
        parseProxyUrl(_proxy_url, _proxy_host, _proxy_port, _proxy_auth);
        _used_proxy = true;
    } else {
        _used_proxy = false;
    }
}

bool HttpClient::checkProxyConnected(const char *data, size_t len) {
    auto ret = strstr(data, "HTTP/1.1 200 Connection established");
    _proxy_connected = ret != nullptr;
    return _proxy_connected;
}

void HttpClient::setAllowResendRequest(bool allow) {
    _allow_resend_request = allow;
}
} /* namespace mediakit */
