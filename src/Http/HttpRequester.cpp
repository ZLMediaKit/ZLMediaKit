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

using namespace std;
using namespace toolkit;

namespace mediakit {

void HttpRequester::onResponseHeader(const string &status, const HttpHeader &headers) {
    _res_body.clear();
}

void HttpRequester::onResponseBody(const char *buf, size_t size) {
    _res_body.append(buf, size);
}

void HttpRequester::onResponseCompleted(const SockException &ex) {
    const_cast<Parser &>(response()).setContent(std::move(_res_body));
    if (_on_result) {
        _on_result(ex, response());
        _on_result = nullptr;
    }
}

void HttpRequester::startRequester(const string &url, const HttpRequesterResult &on_result, float timeout_sec) {
    _on_result = on_result;
    setCompleteTimeout(timeout_sec * 1000);
    sendRequest(url);
}

void HttpRequester::clear() {
    HttpClientImp::clear();
    _res_body.clear();
    _on_result = nullptr;
}

void HttpRequester::setOnResult(const HttpRequesterResult &onResult) {
    _on_result = onResult;
}

} // namespace mediakit
