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
#include "HttpProtocolHint.h"

using namespace std;
using namespace toolkit;

namespace mediakit {

static bool connectionContainsClose(const string &connection) {
    for (auto token : split(connection, ",")) {
        if (strToLower(trim(std::move(token))) == "close") {
            return true;
        }
    }
    return false;
}

void HttpClient::sendRequest(const string &url) {
    sendRequestInternal(url, true);
}

void HttpClient::sendRequestInternal(const string &url, bool reset_replay_body) {
    resetQuicRequest(false);
    if (reset_replay_body) {
        resetReplayableRequestBody();
        prepareReplayableRequestBody();
    }
    auto plan = makeRequestPlan(url);
    auto action = selectTransportAction(plan);
    if (action == HttpClientTransportAction::ConnectQuic) {
        detachTcpTransport();
    }
    clearResponse();
    _active_plan = plan;
    _quic_request_auto = false;
    _quic_auto_fallback_attempted = false;
    applyRequestPlan(plan);
    switch (action) {
    case HttpClientTransportAction::ConnectQuic:
        beginQuicConnect(plan);
        return;
    case HttpClientTransportAction::ConnectProxyTunnel:
        beginProxyConnect(plan);
        return;
    case HttpClientTransportAction::ConnectDirect:
        beginDirectConnect(plan);
        return;
    case HttpClientTransportAction::ReuseConnection:
    default: {
        reuseTransport(plan);
        return;
    }
    }
}

HttpClientRequestPlan HttpClient::makeRequestPlan(const string &url) {
    HttpClientRequestPlan plan;
    plan.url = url;

    auto protocol = findSubString(url.data(), nullptr, "://");
    if (strcasecmp(protocol.data(), "http") == 0) {
        plan.port = 80;
        plan.scheme = HttpClientScheme::Http;
    } else if (strcasecmp(protocol.data(), "https") == 0) {
        plan.port = 443;
        plan.scheme = HttpClientScheme::Https;
    } else {
        auto strErr = StrPrinter << "非法的http url:" << url << endl;
        throw std::invalid_argument(strErr);
    }

    auto host = findSubString(url.data(), "://", "/");
    if (host.empty()) {
        host = findSubString(url.data(), "://", nullptr);
    }

    plan.path = findSubString(url.data(), host.data(), nullptr);
    if (plan.path.empty()) {
        plan.path = "/";
    }
    plan.method = _method.empty() ? "GET" : _method;

    // Reset request headers so the previous request cannot leak into the next one.
    plan.header = _user_set_header;
    auto pos = host.find('@');
    if (pos != string::npos) {
        auto auth_str = host.substr(0, pos);
        host = host.substr(pos + 1, host.size());
        plan.header.emplace("Authorization", "Basic " + encodeBase64(auth_str));
    }

    plan.host_header = host;
    plan.host = host;
    splitUrl(plan.host, plan.host, plan.port);
    plan.transport_host = plan.host;
    plan.transport_port = plan.port;

    plan.keep_alive = _request_keep_alive;
    plan.persistent = _http_persistent && plan.keep_alive;
    plan.protocol_changed = (_is_https != (plan.scheme == HttpClientScheme::Https));
    plan.last_host = plan.host + ":" + to_string(plan.port);
    plan.host_changed = (_last_host != plan.last_host) || plan.protocol_changed;

    plan.header.emplace("Host", plan.host_header);
    plan.header.emplace("User-Agent", kServerName);
    plan.header.emplace("Accept", "*/*");
    plan.header.emplace("Accept-Language", "zh-CN,zh;q=0.8");
    plan.header.emplace("Connection", plan.keep_alive ? "keep-alive" : "close");
    if (!_enable_http3 && _enable_http3_auto && supportsHttp3Transport() && !isUsedProxy() &&
        plan.scheme == HttpClientScheme::Https) {
        HttpAltSvcEndpoint endpoint;
        if (lookupAltSvc(plan.host, plan.port, &endpoint) && canReplayRequestBody()) {
            plan.transport_host = endpoint.host;
            plan.transport_port = endpoint.port;
            plan.auto_http3 = true;
        }
    }

    if (_body && _body->remainSize()) {
        plan.header.emplace("Content-Length", to_string(_body->remainSize()));
        GET_CONFIG(string, charSet, Http::kCharSet);
        plan.header.emplace("Content-Type", "application/x-www-form-urlencoded; charset=" + charSet);
    }

    auto cookies = HttpCookieStorage::Instance().get(plan.last_host, plan.path);
    _StrPrinter printer;
    for (auto &cookie : cookies) {
        printer << cookie->getKey() << "=" << cookie->getVal() << ";";
    }
    if (!printer.empty()) {
        printer.pop_back();
        plan.header.emplace("Cookie", printer);
    }

    return plan;
}

HttpClientTransportAction HttpClient::selectTransportAction(const HttpClientRequestPlan &plan) const {
    if (_enable_http3 && plan.scheme == HttpClientScheme::Https && !isUsedProxy()) {
        if (supportsHttp3Transport()) {
            return HttpClientTransportAction::ConnectQuic;
        }
        WarnL << "HTTP/3 was requested for " << plan.url << " but ENABLE_QUIC is off; falling back to HTTPS/TCP";
    }
    if (plan.auto_http3 && !isUsedProxy() && supportsHttp3Transport()) {
        return HttpClientTransportAction::ConnectQuic;
    }
    if (isUsedProxy()) {
        bool proxy_reuse = TcpClient::alive() && plan.persistent && !plan.host_changed && _proxy_connected;
        return proxy_reuse ? HttpClientTransportAction::ReuseConnection
                           : HttpClientTransportAction::ConnectProxyTunnel;
    }
    if (!TcpClient::alive() || plan.host_changed || !plan.persistent) {
        return HttpClientTransportAction::ConnectDirect;
    }
    return HttpClientTransportAction::ReuseConnection;
}

void HttpClient::beginDirectConnect(const HttpClientRequestPlan &plan) {
    _http_persistent = plan.keep_alive;
    startConnect(plan.host, plan.port, _wait_header_ms / 1000.0f);
}

void HttpClient::beginProxyConnect(const HttpClientRequestPlan &plan) {
    _http_persistent = plan.keep_alive;
    _proxy_connected = false;
    startConnectWithProxy(plan.host, _proxy_host, _proxy_port, _wait_header_ms / 1000.0f);
}

void HttpClient::reuseTransport(const HttpClientRequestPlan &) {
    SockException ex;
    onConnect_l(ex);
}

void HttpClient::applyRequestPlan(const HttpClientRequestPlan &plan) {
    _url = plan.url;
    _path = plan.path;
    _method = plan.method;
    _header = plan.header;
    _last_host = plan.last_host;
    _is_https = (plan.scheme == HttpClientScheme::Https);
}

string HttpClient::makeHttpRequestHeader() const {
    _StrPrinter printer;
    printer << (_method.empty() ? "GET" : _method) + " " << _path + " HTTP/1.1\r\n";
    for (auto &pr : _header) {
        printer << pr.first + ": ";
        printer << pr.second + "\r\n";
    }
    return printer << "\r\n";
}

string HttpClient::makeProxyConnectHeader() const {
    _StrPrinter printer;
    printer << "CONNECT " << _last_host << " HTTP/1.1\r\n";
    printer << "Host: " << _last_host << "\r\n";
    printer << "User-Agent: " << kServerName << "\r\n";
    printer << "Proxy-Connection: keep-alive\r\n";
    if (!_proxy_auth.empty()) {
        printer << "Proxy-Authorization: Basic " << _proxy_auth << "\r\n";
    }
    return printer << "\r\n";
}

ssize_t HttpClient::sendRequestHeaderData(const string &data) {
    return SockSender::send(data);
}

ssize_t HttpClient::sendRequestBodyData(Buffer::Ptr buffer) {
    return send(std::move(buffer));
}

void HttpClient::loadResponseParser(const string &protocol, const string &status,
                                    const string &status_str, const HttpHeader &headers) {
    _StrPrinter printer;
    auto effective_protocol = protocol.empty() ? "HTTP/3" : protocol;
    auto effective_status = status.empty() ? "500" : status;
    auto effective_status_str = status_str.empty() ? "UNKNOWN" : status_str;
    printer << effective_protocol << " " << effective_status << " " << effective_status_str << "\r\n";
    for (auto &pr : headers) {
        printer << pr.first << ": " << pr.second << "\r\n";
    }
    std::string serialized = printer;
    serialized.append("\r\n");
    _parser.parse(serialized.data(), serialized.size());
}

HttpClient::~HttpClient() {
    stopQuicManagerTimer();
    resetQuicRequest(true);
}

void HttpClient::clear() {
    resetQuicRequest(!_request_keep_alive);
    resetReplayableRequestBody();
    _url.clear();
    _user_set_header.clear();
    _body.reset();
    _method.clear();
    _request_keep_alive = true;
    // Keep transport-level state so a live direct/proxy connection can still
    // be reused after the caller resets only the per-request state.
    clearResponse();
    // A reused idle transport can still report a late close after the previous
    // request has already completed. Keep the request state marked as complete
    // until the next sendRequest() resets it, otherwise that late close can be
    // misattributed to the next request.
    _complete = true;
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
    resetReplayableRequestBody();
    _body.reset(new HttpStringBody(std::move(body)));
}

void HttpClient::setBody(HttpBody::Ptr body) {
    resetReplayableRequestBody();
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
    onTransportReady();
}

void HttpClient::onTransportReady() {
    // No proxy is used or the proxy tunnel has already been established.
    if (_proxy_connected || !isUsedProxy()) {
        auto request = makeHttpRequestHeader();
        _header.clear();
        _path.clear();
        sendRequestHeaderData(request);
        onFlush();
        return;
    }
    sendRequestHeaderData(makeProxyConnectHeader());
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
        // If it is a persistent connection, we can solve this problem by reconnecting
        // The connection was reset, possibly because the server actively disconnected the connection,
        // or the persistent connection idle time of the server kernel parameters or firewall timed out or inconsistent.
        // If it is a persistent connection, then we can solve this problem by reconnecting
        WarnL << "http persistent connect reset, try reconnect";
        _http_persistent = false;
        if (_request_body_has_content && !restoreReplayableRequestBody()) {
            onResponseCompleted_l(SockException(Err_other, "http request body is not replayable after connection reset"));
            return;
        }
        sendRequestInternal(_url, false);
        return;
    }
    onResponseCompleted_l(ex);
}

ssize_t HttpClient::onRecvHeader(const char *data, size_t len) {
    _parser.parse(data, len);
    return handleParsedResponseHeader(false);
}

ssize_t HttpClient::handleParsedResponseHeader(bool end_stream) {
    auto connection_close = connectionContainsClose(_parser["Connection"]);
    if (connection_close) {
        _http_persistent = false;
    }
    if (_parser.status() == "302" || _parser.status() == "301" || _parser.status() == "303" || _parser.status() == "307") {
        auto new_url = Parser::mergeUrl(_url, _parser["Location"]);
        if (new_url.empty()) {
            throw invalid_argument("未找到Location字段(跳转url)");
        }
        bool temporary_redirect = _parser.status() == "302" || _parser.status() == "307";
        if (onRedirectUrl(new_url, temporary_redirect)) {
            if (_request_body_has_content && !restoreReplayableRequestBody()) {
                throw invalid_argument("http request body is not replayable for redirect");
            }
            sendRequestInternal(new_url, false);
            return 0;
        }
    }

    checkCookie(_parser.getHeader());
    auto alt_svc = _parser["Alt-Svc"];
    if (!alt_svc.empty()) {
        updateAltSvcCache(_active_plan.host, _active_plan.port, alt_svc);
    }
    onResponseHeader(_parser.status(), _parser.getHeader());
    _header_recved = true;

    if (_parser["Transfer-Encoding"] == "chunked") {
        // 如果Transfer-Encoding字段等于chunked，则认为后续的content是不限制长度的  [AUTO-TRANSLATED:ebbcb35c]
        // If the Transfer-Encoding field is equal to chunked, it is considered that the subsequent content is unlimited in length
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
        // 后续为源源不断的body  [AUTO-TRANSLATED:bf551bbd]
        // The following is a continuous body
        return -1;
    }

    if (!_parser["Content-Length"].empty()) {
        // 有Content-Length字段时忽略onResponseHeader的返回值  [AUTO-TRANSLATED:50380ba8]
        // Ignore the return value of onResponseHeader when there is a Content-Length field
        _total_body_size = atoll(_parser["Content-Length"].data());
    } else {
        _total_body_size = -1;
    }

    if (_total_body_size == 0 || _method == "HEAD") {
        // 后续没content，本次http请求结束  [AUTO-TRANSLATED:8532172f]
        // There is no content afterwards, this http request ends
        onResponseCompleted_l(SockException(Err_success, "The request is successful but has no body"));
        return 0;
    }

    if (end_stream) {
        if (_total_body_size < 0) {
            _total_body_size = 0;
            onResponseCompleted_l(SockException(Err_success, "completed"));
            return 0;
        }
        if (_total_body_size > 0) {
            onResponseCompleted_l(SockException(Err_other, "response body ended before Content-Length was satisfied"));
            return 0;
        }
    }

    // 当_total_body_size != 0时到达这里，代表后续有content  [AUTO-TRANSLATED:3a55b268]
    // When _total_body_size != 0, it means there is content afterwards
    // 虽然我们在_total_body_size >0 时知道content的确切大小，  [AUTO-TRANSLATED:af91f74f]
    // Although we know the exact size of the content when _total_body_size > 0,
    // 但是由于我们没必要等content接收完毕才回调onRecvContent(因为这样浪费内存并且要多次拷贝数据)  [AUTO-TRANSLATED:fd71692c]
    // But because we don't need to wait for the content to be received before calling onRecvContent (because this wastes memory and requires multiple data copies)
    // 所以返回-1代表我们接下来分段接收content  [AUTO-TRANSLATED:388756f6]
    // So returning -1 means we will receive the content in segments next
    _recved_body_size = 0;
    return -1;
}

void HttpClient::onRecvContent(const char *data, size_t len) {
    handleResponseContentData(data, len, false);
}

void HttpClient::handleResponseContentData(const char *data, size_t len, bool fin) {
    if (_chunked_splitter) {
        _chunked_splitter->input(data, len);
        return;
    }
    _recved_body_size += len;
    if (_total_body_size < 0) {
        // 不限长度的content  [AUTO-TRANSLATED:325a9dbc]
        // Unlimited length content
        if (len > 0) {
            onResponseBody(data, len);
        }
        if (fin) {
            _total_body_size = _recved_body_size;
            onResponseCompleted_l(SockException(Err_success, "completed"));
        }
        return;
    }

    // 固定长度的content  [AUTO-TRANSLATED:4d169746]
    // Fixed length content
    if (_recved_body_size < (size_t) _total_body_size) {
        // content还未接收完毕  [AUTO-TRANSLATED:b30ca92c]
        // Content has not been received yet
        if (len > 0) {
            onResponseBody(data, len);
        }
        if (fin) {
            onResponseCompleted_l(SockException(Err_other, "response body ended before Content-Length was satisfied"));
        }
        return;
    }

    if (_recved_body_size == (size_t)_total_body_size) {
        // content接收完毕  [AUTO-TRANSLATED:e730ea8c]
        // Content received
        if (len > 0) {
            onResponseBody(data, len);
        }
        onResponseCompleted_l(SockException(Err_success, "completed"));
        return;
    }

    // 声明的content数据比真实的小，断开链接  [AUTO-TRANSLATED:38204302]
    // The declared content data is smaller than the real one, disconnect
    if (len > 0) {
        onResponseBody(data, len);
    }
    throw invalid_argument("http response content size bigger than expected");
}

void HttpClient::onFlush() {
    GET_CONFIG(uint32_t, send_buf_size, Http::kSendBufSize);
    while (_body && _body->remainSize() && !isSocketBusy()) {
        auto buffer = _body->readData(send_buf_size);
        if (!buffer) {
            // 数据发送结束或读取数据异常  [AUTO-TRANSLATED:75179972]
            // Data transmission ends or data reading exception
            break;
        }
        if (sendRequestBodyData(buffer) <= 0) {
            // 发送数据失败，不需要回滚数据，因为发送前已经通过isSocketBusy()判断socket可写  [AUTO-TRANSLATED:30762202]
            // Data transmission failed, no need to roll back data, because the socket is writable before sending
            // 所以发送缓存区肯定未满,该buffer肯定已经写入socket  [AUTO-TRANSLATED:769fff52]
            // So the send buffer is definitely not full, this buffer must have been written to the socket
            break;
        }
    }
}

void HttpClient::onManager() {
    // onManager回调在连接中或已连接状态才会调用  [AUTO-TRANSLATED:acf86dce]
    // The onManager callback is only called when the connection is in progress or connected

    if (_wait_complete_ms > 0) {
        // 设置了总超时时间  [AUTO-TRANSLATED:ac47c234]
        // Total timeout is set
        if (!_complete && _wait_complete.elapsedTime() > _wait_complete_ms) {
            // 等待http回复完毕超时  [AUTO-TRANSLATED:711ebc7b]
            // Timeout waiting for http reply to finish
            shutdown(SockException(Err_timeout, "wait http response complete timeout"));
            return;
        }
        return;
    }

    // 未设置总超时时间  [AUTO-TRANSLATED:a936338f]
    // Total timeout is not set
    if (!_header_recved) {
        // 等待header中  [AUTO-TRANSLATED:f8635de6]
        // Waiting for header
        if (_wait_header.elapsedTime() > _wait_header_ms) {
            // 等待header中超时  [AUTO-TRANSLATED:860d3a16]
            // Timeout waiting for header
            shutdown(SockException(Err_timeout, "wait http response header timeout"));
            return;
        }
    } else if (_wait_body_ms > 0 && _wait_body.elapsedTime() > _wait_body_ms) {
        // 等待body中，等待超时  [AUTO-TRANSLATED:f9bb1d66]
        // Waiting for body, timeout
        shutdown(SockException(Err_timeout, "wait http response body timeout"));
        return;
    }
}

void HttpClient::shutdown(const SockException &ex) {
    auto had_quic_request = _quic_request_active;
    resetQuicRequest(true);
    stopQuicManagerTimer();
    TcpClient::shutdown(ex);
    if (had_quic_request) {
        onResponseCompleted_l(ex);
    }
}

void HttpClient::onResponseCompleted_l(const SockException &ex) {
    if (_complete) {
        return;
    }
    _complete = true;
    _wait_complete.resetTime();

    if (!ex) {
        // 确认无疑的成功  [AUTO-TRANSLATED:e1db8ce2]
        // Confirmed success
        onResponseCompleted(ex);
        return;
    }
    // 可疑的失败  [AUTO-TRANSLATED:1258a436]
    // Suspicious failure

    if (_total_body_size > 0 && _recved_body_size >= (size_t)_total_body_size) {
        // 回复header中有content-length信息，那么收到的body大于等于声明值则认为成功  [AUTO-TRANSLATED:2f813650]
        // If the response header contains content-length information, then the received body is considered successful if it is greater than or equal to the declared value
        onResponseCompleted(SockException(Err_success, "read body completed"));
        return;
    }

    if (_total_body_size == -1 && _recved_body_size > 0) {
        // 回复header中无content-length信息，那么收到一点body也认为成功  [AUTO-TRANSLATED:6c0e87fc]
        // If the response header does not contain content-length information, then receiving any body is considered successful
        onResponseCompleted(SockException(Err_success, ex.what()));
        return;
    }

    // 确认无疑的失败  [AUTO-TRANSLATED:33b216d9]
    // Confirmed failure
    onResponseCompleted(ex);
}

bool HttpClient::waitResponse() const {
    return !_complete && alive();
}

bool HttpClient::alive() const {
    return TcpClient::alive() || isQuicTransportAlive();
}

bool HttpClient::isHttps() const {
    return _is_https;
}

void HttpClient::checkCookie(HttpClient::HttpHeader &headers) {
    //Set-Cookie: IPTV_SERVER=8E03927B-CC8C-4389-BC00-31DBA7EC7B49;expires=Sun, Sep 23 2018 15:07:31 GMT;path=/index/api/
    for (auto it_set_cookie = headers.find("Set-Cookie"); it_set_cookie != headers.end(); ++it_set_cookie) {
        HttpCookie::Ptr cookie = std::make_shared<HttpCookie>();
        cookie->setHost(_last_host);

        int index = 0;
        auto arg_vec = split(it_set_cookie->second, ";");
        for (string &cookie_item : arg_vec) {
            auto key = findSubString(cookie_item.data(), NULL, "=");
            auto val = findSubString(cookie_item.data(), "=", NULL);

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
            // 无效的cookie  [AUTO-TRANSLATED:5f06aec8]
            // Invalid cookie
            continue;
        }
        HttpCookieStorage::Instance().set(cookie);
    }
}

void HttpClient::detachTcpTransport() {
    auto sock = getSock();
    if (!sock) {
        _proxy_connected = false;
        return;
    }
    // Detach callbacks before dropping the old transport. When we switch from
    // an HTTPS/TCP bootstrap connection to QUIC, the previous keep-alive
    // socket can still report a late EOF. That stale TCP error must not be
    // allowed to complete the new HTTP/3 request.
    sock->setOnRead([](Buffer::Ptr &, struct sockaddr *, int) {});
    sock->setOnErr([](const SockException &) {});
    sock->setOnFlush(nullptr);
    sock->emitErr(SockException(Err_shutdown, "switch transport"));
    setSock(nullptr);
    _proxy_connected = false;
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

void HttpClient::setRequestKeepAlive(bool enable) {
    _request_keep_alive = enable;
    if (!enable) {
        _http_persistent = false;
    }
}

bool HttpClient::isUsedProxy() const {
    return _used_proxy;
}

bool HttpClient::isProxyConnected() const {
    return _proxy_connected;
}

void HttpClient::setProxyUrl(string proxy_url) {
    auto old_used_proxy = _used_proxy;
    auto old_proxy_host = _proxy_host;
    auto old_proxy_port = _proxy_port;
    auto old_proxy_auth = _proxy_auth;

    _proxy_url = std::move(proxy_url);
    if (!_proxy_url.empty()) {
        _proxy_host.clear();
        _proxy_port = 0;
        _proxy_auth.clear();
        parseProxyUrl(_proxy_url, _proxy_host, _proxy_port, _proxy_auth);
        _used_proxy = true;
    } else {
        _used_proxy = false;
        _proxy_host.clear();
        _proxy_port = 0;
        _proxy_auth.clear();
        _proxy_connected = false;
    }

    auto proxy_config_changed = old_used_proxy != _used_proxy
                                || old_proxy_host != _proxy_host
                                || old_proxy_port != _proxy_port
                                || old_proxy_auth != _proxy_auth;
    if (proxy_config_changed) {
        // A proxy mode or endpoint change must not reuse the previous transport.
        _http_persistent = false;
        _proxy_connected = false;
    }
}

void HttpClient::setEnableHttp3(bool enable) {
    _enable_http3 = enable;
    if (!enable) {
        resetQuicRequest(true);
    }
}

void HttpClient::setEnableHttp3Auto(bool enable) {
    _enable_http3_auto = enable;
}

void HttpClient::setHttp3VerifyPeer(bool verify) {
    _http3_verify_peer = verify;
}

void HttpClient::setHttp3CAFile(std::string ca_file) {
    _http3_ca_file = std::move(ca_file);
}

bool HttpClient::checkProxyConnected(const char *data, size_t len) {
    string response(data, len);
    if (response.find("HTTP/1.1 200") != string::npos || response.find("HTTP/1.0 200") != string::npos) {
        _proxy_connected = true;
        return true;
    }

    _proxy_connected = false;
    // CONNECT failed, which usually means the proxy rejected the tunnel request,
    // does not support CONNECT for this target, or the proxy authentication is invalid.
    WarnL << "proxy CONNECT failed, status line: "
          << response.substr(0, response.find("\r\n"));
    return false;
}

void HttpClient::setAllowResendRequest(bool allow) {
    _allow_resend_request = allow;
}

void HttpClient::stopQuicManagerTimer() {
    _quic_timer.reset();
}

bool HttpClient::prepareReplayableRequestBody() {
    resetReplayableRequestBody();
    if (!_body) {
        return true;
    }

    auto remain = _body->remainSize();
    if (remain < 0) {
        _request_body_has_content = true;
        return false;
    }
    if (remain == 0) {
        return true;
    }
    _request_body_has_content = true;

    GET_CONFIG(uint32_t, replay_max_size, Http::kHttp3AutoReplayMaxSize);
    if (static_cast<uint64_t>(remain) > replay_max_size) {
        return false;
    }

    std::string snapshot;
    if (!_body->snapshot(snapshot, replay_max_size)) {
        return false;
    }

    _request_body_replay = std::move(snapshot);
    _request_body_replay_ready = true;
    _body = std::make_shared<HttpStringBody>(_request_body_replay);
    return true;
}

void HttpClient::resetReplayableRequestBody() {
    _request_body_has_content = false;
    _request_body_replay_ready = false;
    _request_body_replay.clear();
}

bool HttpClient::restoreReplayableRequestBody() {
    if (!_request_body_has_content) {
        return true;
    }
    if (!_request_body_replay_ready) {
        return false;
    }
    _body = std::make_shared<HttpStringBody>(_request_body_replay);
    return true;
}

bool HttpClient::canReplayRequestBody() const {
    return !_request_body_has_content || _request_body_replay_ready;
}

bool HttpClient::canFallbackToDirectFromAutoHttp3() const {
    if (!_quic_request_committed) {
        return true;
    }

    if (_request_body_has_content) {
        return false;
    }

    auto method = _active_plan.method;
    strToUpper(method);
    return method == "GET" || method == "HEAD" || method == "OPTIONS";
}

HttpClientRequestPlan HttpClient::makeDirectFallbackPlan() const {
    auto plan = _active_plan;
    plan.auto_http3 = false;
    plan.transport_host = plan.host;
    plan.transport_port = plan.port;
    return plan;
}

} /* namespace mediakit */
