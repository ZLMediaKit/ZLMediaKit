/*
 * Copyright (c) 2016-present The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/ZLMediaKit/ZLMediaKit).
 *
 * Use of this source code is governed by MIT-like license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#ifndef Htt_HttpRequester_h
#define Htt_HttpRequester_h

#include "HttpClientImp.h"

namespace mediakit {

class HttpRequester : public HttpClientImp {
public:
    using Ptr = std::shared_ptr<HttpRequester>;
    using HttpRequesterResult = std::function<void(const toolkit::SockException &ex, const Parser &response)>;

    void setOnResult(const HttpRequesterResult &onResult);
    void startRequester(const std::string &url, const HttpRequesterResult &on_result, float timeout_sec = 10);
    void setRetry(size_t count, size_t delay);
    size_t getRetry() const { return _retry; }
    size_t getRetryDelay() const { return _retry_delay; }
    size_t getMaxRetry() const { return _max_retry; }
    void clear() override;

private:
    void onResponseHeader(const std::string &status, const HttpHeader &headers) override;
    void onResponseBody(const char *buf, size_t size) override;
    void onResponseCompleted(const toolkit::SockException &ex) override;

private:
    size_t _retry = 0;
    size_t _max_retry = 0;
    size_t _retry_delay = 2000; // ms
    std::string _res_body;
    HttpRequesterResult _on_result;
};

}//namespace mediakit

#endif /* Htt_HttpRequester_h */
