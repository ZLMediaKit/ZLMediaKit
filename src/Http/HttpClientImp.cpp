/*
 * Copyright (c) 2016-present The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/ZLMediaKit/ZLMediaKit).
 *
 * Use of this source code is governed by MIT-like license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#include "Http/HttpClientImp.h"

using namespace toolkit;

namespace mediakit {

void HttpClientImp::onConnect(const SockException &ex) {
    if (isUsedProxy() && !isProxyConnected()) {
        // 连接代理服务器
        setDoNotUseSSL();
        HttpClient::onConnect(ex);
    } else {
        if (!isHttps()) {
            // https 302跳转 http时，需要关闭ssl
            setDoNotUseSSL();
            HttpClient::onConnect(ex);
        } else {
            TcpClientWithSSL<HttpClient>::onConnect(ex);
        }
    }
}

ssize_t HttpClientImp::onRecvHeader(const char *data, size_t len) {
    if (isUsedProxy() && !isProxyConnected()) {
        if (checkProxyConnected(data, len)) {
            clearResponse();
            onConnect(SockException(Err_success, "proxy connected"));
            return 0;
        }
    }
    return HttpClient::onRecvHeader(data, len);
}

} /* namespace mediakit */
