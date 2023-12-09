/*
 * Copyright (c) 2020 The ZLMediaKit project authors. All Rights Reserved.
 * Created by alex on 2021/4/6.
 * This file is part of ZLMediaKit(https://github.com/ZLMediaKit/ZLMediaKit).
 *
 * Use of this source code is governed by MIT-like license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#include "TsPlayer.h"
#include "Common/config.h"
using namespace std;
using namespace toolkit;

namespace mediakit {

TsPlayer::TsPlayer(const EventPoller::Ptr &poller) : HttpTSPlayer(poller) {}

void TsPlayer::play(const string &url) {
    TraceL << "play http-ts: " << url;
    _play_result = false;
    _benchmark_mode = (*this)[Client::kBenchmarkMode].as<int>();
    setProxyUrl((*this)[Client::kProxyUrl]);
    setHeaderTimeout((*this)[Client::kTimeoutMS].as<int>());
    setBodyTimeout((*this)[Client::kMediaTimeoutMS].as<int>());
    setMethod("GET");
    sendRequest(url);
}

void TsPlayer::teardown() {
    shutdown(SockException(Err_shutdown, "teardown"));
}

void TsPlayer::onResponseCompleted(const SockException &ex) {
    if (!_play_result) {
        _play_result = true;
        if (!ex && responseBodyTotalSize() == 0 && responseBodySize() == 0) {
            //if the server does not return any data, it is considered a failure
            onShutdown(ex);
        } else {
            onPlayResult(ex);
        }
    } else {
        onShutdown(ex);
    }
    HttpTSPlayer::onResponseCompleted(ex);
}

void TsPlayer::onResponseBody(const char *buf, size_t size) {
    if (!_play_result) {
        _play_result = true;
        onPlayResult(SockException(Err_success, "read http-ts stream successfully"));
    }
    if (!_benchmark_mode) {
        HttpTSPlayer::onResponseBody(buf, size);
    }
}

} // namespace mediakit