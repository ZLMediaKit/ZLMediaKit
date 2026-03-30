/*
 * Copyright (c) 2016-present The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/ZLMediaKit/ZLMediaKit).
 *
 * Use of this source code is governed by MIT-like license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#include "HttpClient.h"

#if defined(ENABLE_QUIC)

#include "Common/config.h"
#include "HttpProtocolHint.h"
#include "quic/QuicCongestionConfig.h"
#include "quic/QuicClientBackend.h"

using namespace std;
using namespace toolkit;

namespace mediakit {

static std::string normalizeHttp3HeaderName(const std::string &name) {
    auto copy = name;
    return strToLower(copy);
}

void HttpClient::beginQuicConnect(const HttpClientRequestPlan &plan) {
    detachTcpTransport();
    _http_persistent = plan.keep_alive;
    _quic_request_auto = plan.auto_http3 && !_enable_http3;
    ensureQuicBackend();
    if (!_quic_backend || !_quic_backend->available()) {
        if (_quic_request_auto) {
            eraseAltSvc(plan.host, plan.port);
            _quic_request_auto = false;
            beginDirectConnect(makeDirectFallbackPlan());
            return;
        }
        onResponseCompleted_l(SockException(Err_other, "http3 backend unavailable"));
        return;
    }

    auto restart_backend = !_quic_backend->alive() || _quic_host != plan.transport_host || _quic_port != plan.transport_port;
    if (restart_backend) {
        if (_quic_backend->alive()) {
            _quic_backend->shutdown();
        }

        QuicClientConfig config;
        config.verify_peer = _http3_verify_peer;
        config.connect_timeout_ms = _wait_header_ms;
        config.idle_timeout_ms = _wait_body_ms ? _wait_body_ms : _wait_header_ms;
        config.congestion_algo = loadClientQuicCongestionAlgoConfig();
        if (!_http3_ca_file.empty()) {
            config.ca_file.data = _http3_ca_file.data();
            config.ca_file.len = _http3_ca_file.size();
        }
        if (_quic_backend->start(plan.transport_host, plan.transport_port, config) != 0) {
            if (_quic_request_auto) {
                eraseAltSvc(plan.host, plan.port);
                _quic_request_auto = false;
                restoreAutoHttp3ReplayBody();
                beginDirectConnect(makeDirectFallbackPlan());
                return;
            }
            onResponseCompleted_l(SockException(Err_other, "start http3 transport failed"));
            return;
        }
        _quic_host = plan.transport_host;
        _quic_port = plan.transport_port;
        _quic_authority = plan.host_header;
    }

    QuicClientRequest request;
    request.request_id = ++_quic_request_id;
    request.authority = plan.host_header;
    request.path = plan.path;
    request.method = plan.method;
    request.end_stream = !_body || _body->remainSize() == 0;
    for (auto &pr : plan.header) {
        auto name = normalizeHttp3HeaderName(pr.first);
        if (name == "host" || name == "connection" || name == "proxy-connection") {
            continue;
        }
        request.headers.emplace_back(QuicClientOwnedHeader{name, pr.second});
    }

    _quic_request_active = true;
    startQuicManagerTimer();
    if (_quic_backend->startRequest(request) != 0) {
        _quic_request_active = false;
        stopQuicManagerTimer();
        if (_quic_request_auto) {
            eraseAltSvc(plan.host, plan.port);
            _quic_request_auto = false;
            restoreAutoHttp3ReplayBody();
            beginDirectConnect(makeDirectFallbackPlan());
            return;
        }
        onResponseCompleted_l(SockException(Err_other, "start http3 request failed"));
        return;
    }

    GET_CONFIG(uint32_t, send_buf_size, Http::kSendBufSize);
    while (_body && _body->remainSize()) {
        auto buffer = _body->readData(send_buf_size);
        if (!buffer) {
            break;
        }
        auto fin = !_body->remainSize();
        if (_quic_backend->sendBody(request.request_id, reinterpret_cast<const uint8_t *>(buffer->data()), buffer->size(), fin) != 0) {
            _quic_request_active = false;
            stopQuicManagerTimer();
            if (_quic_request_auto) {
                eraseAltSvc(plan.host, plan.port);
                _quic_request_auto = false;
                restoreAutoHttp3ReplayBody();
                beginDirectConnect(makeDirectFallbackPlan());
                return;
            }
            onResponseCompleted_l(SockException(Err_other, "send http3 request body failed"));
            return;
        }
    }
}

void HttpClient::ensureQuicBackend() {
    if (_quic_backend) {
        return;
    }
    _quic_backend = std::make_shared<QuicClientBackend>(getPoller());
    if (!_quic_backend->available()) {
        _quic_backend.reset();
        return;
    }

    std::weak_ptr<HttpClient> weak_self = std::static_pointer_cast<HttpClient>(shared_from_this());
    QuicClientBackend::Callbacks callbacks;
    callbacks.on_log = [weak_self](QuicLogLevel level, const std::string &message) {
        auto self = weak_self.lock();
        if (!self) {
            return;
        }
        self->onQuicBackendLog(static_cast<int>(level), message);
    };
    callbacks.on_headers = [weak_self](const QuicClientResponseHeaders &headers) {
        auto self = weak_self.lock();
        if (!self) {
            return;
        }
        HttpHeader map;
        for (auto &header : headers.headers) {
            map.emplace(header.name, header.value);
        }
        self->onQuicResponseHeaders(headers.request_id, headers.status_code, map, headers.fin);
    };
    callbacks.on_body = [weak_self](uint64_t request_id, const uint8_t *data, size_t len, bool fin) {
        auto self = weak_self.lock();
        if (!self) {
            return;
        }
        self->onQuicResponseBody(request_id, reinterpret_cast<const char *>(data), len, fin);
    };
    callbacks.on_close = [weak_self](const QuicClientCloseInfo &close_info) {
        auto self = weak_self.lock();
        if (!self) {
            return;
        }
        self->onQuicRequestClosed(close_info.request_id, static_cast<int>(close_info.state), close_info.reason);
    };
    _quic_backend->setCallbacks(std::move(callbacks));
}

void HttpClient::resetQuicRequest(bool close_transport) {
    stopQuicManagerTimer();
    auto had_active_request = _quic_request_active;
    auto request_id = _quic_request_id;
    _quic_request_active = false;
    if (_quic_backend && had_active_request && !close_transport) {
        _quic_backend->cancelRequest(request_id, 0);
    }
    if (close_transport && _quic_backend) {
        _quic_backend->shutdown();
        _quic_backend.reset();
        _quic_host.clear();
        _quic_authority.clear();
        _quic_port = 0;
    }
}

void HttpClient::startQuicManagerTimer() {
    if (_quic_timer || !_quic_request_active) {
        return;
    }
    std::weak_ptr<HttpClient> weak_self = std::static_pointer_cast<HttpClient>(shared_from_this());
    _quic_timer = std::make_shared<Timer>(0.2f, [weak_self]() {
        auto self = weak_self.lock();
        if (!self || !self->_quic_request_active) {
            return false;
        }

        if (self->_wait_complete_ms > 0) {
            if (!self->_complete && self->_wait_complete.elapsedTime() > self->_wait_complete_ms) {
                if (self->tryFallbackToDirectFromAutoHttp3()) {
                    return false;
                }
                self->shutdown(SockException(Err_timeout, "wait http3 response complete timeout"));
                return false;
            }
            return true;
        }

        if (!self->_header_recved) {
            if (self->_wait_header.elapsedTime() > self->_wait_header_ms) {
                if (self->tryFallbackToDirectFromAutoHttp3()) {
                    return false;
                }
                self->shutdown(SockException(Err_timeout, "wait http3 response header timeout"));
                return false;
            }
            return true;
        }

        if (self->_wait_body_ms > 0 && self->_wait_body.elapsedTime() > self->_wait_body_ms) {
            self->shutdown(SockException(Err_timeout, "wait http3 response body timeout"));
            return false;
        }
        return true;
    }, getPoller());
}

void HttpClient::onQuicResponseHeaders(uint64_t request_id, int32_t status_code, const HttpHeader &headers, bool fin) {
    if (!_quic_request_active || request_id != _quic_request_id) {
        return;
    }

    _wait_header.resetTime();
    _wait_body.resetTime();
    try {
        loadResponseParser("HTTP/3", std::to_string(status_code), "", headers);
        handleParsedResponseHeader(fin);
    } catch (std::exception &ex) {
        onResponseCompleted_l(SockException(Err_other, ex.what()));
    }
}

void HttpClient::onQuicResponseBody(uint64_t request_id, const char *data, size_t len, bool fin) {
    if (!_quic_request_active || request_id != _quic_request_id) {
        return;
    }

    _wait_body.resetTime();
    try {
        handleResponseContentData(data, len, fin);
    } catch (std::exception &ex) {
        onResponseCompleted_l(SockException(Err_other, ex.what()));
    }
}

void HttpClient::onQuicRequestClosed(uint64_t request_id, int state, const std::string &reason) {
    if (!_quic_request_active || request_id != _quic_request_id) {
        return;
    }

    _quic_request_active = false;
    stopQuicManagerTimer();

    if (tryFallbackToDirectFromAutoHttp3()) {
        return;
    }

    if (_complete) {
        if (!_http_persistent && _quic_backend) {
            _quic_backend->shutdown();
        }
        return;
    }

    auto quic_state = static_cast<QuicClientState>(state);
    if (quic_state == QuicClientState::Completed && _header_recved) {
        onResponseCompleted_l(SockException(Err_success, reason.empty() ? "http3 request completed" : reason));
    } else {
        onResponseCompleted_l(SockException(Err_other, reason.empty() ? "http3 request closed" : reason));
    }

    if (!_http_persistent && _quic_backend) {
        _quic_backend->shutdown();
    }
}

bool HttpClient::tryFallbackToDirectFromAutoHttp3() {
    if (!_quic_request_auto || _quic_auto_fallback_attempted || _header_recved || _recved_body_size != 0) {
        return false;
    }

    _quic_auto_fallback_attempted = true;
    _quic_request_auto = false;
    _quic_request_active = false;
    stopQuicManagerTimer();
    eraseAltSvc(_active_plan.host, _active_plan.port);
    if (_quic_backend) {
        _quic_backend->shutdown();
        _quic_host.clear();
        _quic_authority.clear();
        _quic_port = 0;
    }
    restoreAutoHttp3ReplayBody();
    beginDirectConnect(makeDirectFallbackPlan());
    return true;
}

bool HttpClient::supportsHttp3Transport() const {
    return true;
}

bool HttpClient::isQuicTransportAlive() const {
    return _quic_backend && _quic_backend->alive();
}

} // namespace mediakit

#endif // defined(ENABLE_QUIC)
