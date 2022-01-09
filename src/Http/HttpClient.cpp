/*
 * Copyright (c) 2016 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/xia-chu/ZLMediaKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#include <cstdlib>
#include "Util/base64.h"
#include "HttpClient.h"
#include "Common/config.h"

namespace mediakit {

void HttpClient::sendRequest(const string &url, float timeout_sec, float recv_timeout_sec) {
    _recv_timeout_second = recv_timeout_sec;
    clearResponse();
    _url = url;
    auto protocol = FindField(url.data(), NULL, "://");
    uint16_t default_port;
    bool is_https;
    if (strcasecmp(protocol.data(), "http") == 0) {
        default_port = 80;
        is_https = false;
    } else if (strcasecmp(protocol.data(), "https") == 0) {
        default_port = 443;
        is_https = true;
    } else {
        auto strErr = StrPrinter << "非法的http url:" << url << endl;
        throw std::invalid_argument(strErr);
    }

    auto host = FindField(url.data(), "://", "/");
    if (host.empty()) {
        host = FindField(url.data(), "://", NULL);
    }
    _path = FindField(url.data(), host.data(), NULL);
    if (_path.empty()) {
        _path = "/";
    }
    auto pos = host.find('@');
    if (pos != string::npos) {
        //去除？后面的字符串
        auto authStr = host.substr(0, pos);
        host = host.substr(pos + 1, host.size());
        _header.emplace("Authorization", "Basic " + encodeBase64(authStr));
    }
    auto host_header = host;
    uint16_t port = atoi(FindField(host.data(), ":", NULL).data());
    if (port <= 0) {
        //默认端口
        port = default_port;
    } else {
        //服务器域名
        host = FindField(host.data(), NULL, ":");
    }
    _header.emplace("Host", host_header);
    _header.emplace("User-Agent", kServerName);
    _header.emplace("Connection", "keep-alive");
    _header.emplace("Accept", "*/*");
    _header.emplace("Accept-Language", "zh-CN,zh;q=0.8");

    if (_body && _body->remainSize()) {
        _header.emplace("Content-Length", to_string(_body->remainSize()));
        _header.emplace("Content-Type", "application/x-www-form-urlencoded; charset=UTF-8");
    }

    bool host_changed = (_last_host != host + ":" + to_string(port)) || (_is_https != is_https);
    _last_host = host + ":" + to_string(port);
    _is_https = is_https;
    _timeout_second = timeout_sec;

    auto cookies = HttpCookieStorage::Instance().get(_last_host, _path);
    _StrPrinter printer;
    for (auto &cookie : cookies) {
        printer << cookie->getKey() << "=" << cookie->getVal() << ";";
    }
    if (!printer.empty()) {
        printer.pop_back();
        _header.emplace("Cookie", printer);
    }

    if (!alive() || host_changed) {
        startConnect(host, port, timeout_sec);
    } else {
        SockException ex;
        onConnect_l(ex);
    }
}

void HttpClient::clear() {
    _url.clear();
    _header.clear();
    _body.reset();
    _method.clear();
    _path.clear();
    clearResponse();
}

void HttpClient::clearResponse() {
    _complete = false;
    _recved_body_size = 0;
    _total_body_size = 0;
    _parser.Clear();
    _header.clear();
    _chunked_splitter = nullptr;
    _recv_timeout_ticker.resetTime();
    _total_timeout_ticker.resetTime();
    HttpRequestSplitter::reset();
}

void HttpClient::setMethod(string method) {
    _method = std::move(method);
}

void HttpClient::setHeader(HttpHeader header) {
    _header = std::move(header);
}

HttpClient &HttpClient::addHeader(string key, string val, bool force) {
    if (!force) {
        _header.emplace(std::move(key), std::move(val));
    } else {
        _header[std::move(key)] = std::move(val);
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

const string &HttpClient::getUrl() const {
    return _url;
}

void HttpClient::onConnect(const SockException &ex) {
    onConnect_l(ex);
}

void HttpClient::onConnect_l(const SockException &ex) {
    _recv_timeout_ticker.resetTime();
    if (ex) {
        onDisconnect(ex);
        return;
    }

    _StrPrinter printer;
    printer << _method + " " << _path + " HTTP/1.1\r\n";
    for (auto &pr : _header) {
        printer << pr.first + ": ";
        printer << pr.second + "\r\n";
    }
    SockSender::send(printer << "\r\n");
    onFlush();
}

void HttpClient::onRecv(const Buffer::Ptr &pBuf) {
    _recv_timeout_ticker.resetTime();
    HttpRequestSplitter::input(pBuf->data(), pBuf->size());
}

void HttpClient::onErr(const SockException &ex) {
    if (ex.getErrCode() == Err_eof && _total_body_size < 0) {
        //如果Content-Length未指定 但服务器断开链接
        //则认为本次http请求完成
        onResponseCompleted_l();
    }
    onDisconnect(ex);
}

ssize_t HttpClient::onRecvHeader(const char *data, size_t len) {
    _parser.Parse(data);
    if (_parser.Url() == "302" || _parser.Url() == "301") {
        auto new_url = _parser["Location"];
        if (new_url.empty()) {
            shutdown(SockException(Err_shutdown, "未找到Location字段(跳转url)"));
            return 0;
        }
        if (onRedirectUrl(new_url, _parser.Url() == "302")) {
            setMethod("GET");
            HttpClient::sendRequest(new_url, _timeout_second, _recv_timeout_second);
            return 0;
        }
    }

    checkCookie(_parser.getHeader());
    _total_body_size = onResponseHeader(_parser.Url(), _parser.getHeader());

    if (!_parser["Content-Length"].empty()) {
        //有Content-Length字段时忽略onResponseHeader的返回值
        _total_body_size = atoll(_parser["Content-Length"].data());
    }

    if (_parser["Transfer-Encoding"] == "chunked") {
        //如果Transfer-Encoding字段等于chunked，则认为后续的content是不限制长度的
        _total_body_size = -1;
        _chunked_splitter = std::make_shared<HttpChunkedSplitter>([this](const char *data, size_t len) {
            if (len > 0) {
                auto recved_body_size = _recved_body_size + len;
                onResponseBody(data, len, recved_body_size, SIZE_MAX);
                _recved_body_size = recved_body_size;
            } else {
                onResponseCompleted_l();
            }
        });
    }

    if (_total_body_size == 0) {
        //后续没content，本次http请求结束
        onResponseCompleted_l();
        return 0;
    }

    //当_totalBodySize != 0时到达这里，代表后续有content
    //虽然我们在_totalBodySize >0 时知道content的确切大小，
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
    auto recved_body_size = _recved_body_size + len;
    if (_total_body_size < 0) {
        //不限长度的content,最大支持SIZE_MAX个字节
        onResponseBody(data, len, recved_body_size, SIZE_MAX);
        _recved_body_size = recved_body_size;
        return;
    }

    //固定长度的content
    if (recved_body_size < (size_t) _total_body_size) {
        //content还未接收完毕
        onResponseBody(data, len, recved_body_size, _total_body_size);
        _recved_body_size = recved_body_size;
        return;
    }

    //content接收完毕
    onResponseBody(data, _total_body_size - _recved_body_size, _total_body_size, _total_body_size);
    bool bigger_than_expected = recved_body_size > (size_t) _total_body_size;
    onResponseCompleted_l();
    if (bigger_than_expected) {
        //声明的content数据比真实的小，那么我们只截取前面部分的并断开链接
        shutdown(SockException(Err_shutdown, "http response content size bigger than expected"));
    }
}

void HttpClient::onFlush() {
    _recv_timeout_ticker.resetTime();
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
    if (_recv_timeout_ticker.elapsedTime() > _recv_timeout_second * 1000 && _total_body_size < 0 && !_chunked_splitter) {
        //如果Content-Length未指定 但接收数据超时
        //则认为本次http请求完成
        onResponseCompleted_l();
    }

    if (waitResponse() && _timeout_second > 0 && _total_timeout_ticker.elapsedTime() > _timeout_second * 1000) {
        //超时
        shutdown(SockException(Err_timeout, "http request timeout"));
    }
}

void HttpClient::onResponseCompleted_l() {
    _complete = true;
    onResponseCompleted();
}

bool HttpClient::waitResponse() const {
    return !_complete && alive();
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
            auto key = FindField(key_val.data(), NULL, "=");
            auto val = FindField(key_val.data(), "=", NULL);

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

} /* namespace mediakit */
