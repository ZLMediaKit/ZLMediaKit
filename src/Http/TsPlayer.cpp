/*
 * Copyright (c) 2020 The ZLMediaKit project authors. All Rights Reserved.
 * Created by alex on 2021/4/6.
 * This file is part of ZLMediaKit(https://github.com/xia-chu/ZLMediaKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#include "TsPlayer.h"

using namespace std;
using namespace toolkit;

namespace mediakit {

TsPlayer::TsPlayer(const EventPoller::Ptr &poller) : HttpTSPlayer(poller) {}

void TsPlayer::play(const string &url) {
    TraceL << "play http-ts: " << url;
    _play_result = false;
    _benchmark_mode = (*this)[Client::kBenchmarkMode].as<int>();
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
        onPlayResult(ex);
    } else {
        onShutdown(ex);
    }
    HttpTSPlayer::onResponseCompleted(ex);
}

void TsPlayer::onResponseBody(const char *buf, size_t size) {
    if (!_play_result) {
        _play_result = true;
        onPlayResult(SockException(Err_success, "play http-ts success"));
    }
    if (!_benchmark_mode) {
        HttpTSPlayer::onResponseBody(buf, size);
    }
}

} // namespace mediakit