/*
 * Copyright (c) 2016 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/xia-chu/ZLMediaKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#include "HttpRequester.h"

namespace mediakit {

ssize_t HttpRequester::onResponseHeader(const string &status, const HttpHeader &headers) {
    _strRecvBody.clear();
    return HttpClientImp::onResponseHeader(status, headers);
}

void HttpRequester::onResponseBody(const char *buf, size_t size, size_t recvedSize, size_t totalSize) {
    _strRecvBody.append(buf, size);
}

void HttpRequester::onResponseCompleted() {
    const_cast<Parser &> (response()).setContent(std::move(_strRecvBody));
    if (_onResult) {
        _onResult(SockException(), response());
        _onResult = nullptr;
    }
}

void HttpRequester::onDisconnect(const SockException &ex) {
    const_cast<Parser &> (response()).setContent(std::move(_strRecvBody));
    if (_onResult) {
        _onResult(ex, response());
        _onResult = nullptr;
    }
}

void HttpRequester::startRequester(const string &url, const HttpRequesterResult &onResult, float timeOutSecond) {
    _onResult = onResult;
    sendRequest(url, timeOutSecond);
}

void HttpRequester::clear() {
    HttpClientImp::clear();
    _strRecvBody.clear();
    _onResult = nullptr;
}

void HttpRequester::setOnResult(const HttpRequesterResult &onResult) {
    _onResult = onResult;
}


}//namespace mediakit
