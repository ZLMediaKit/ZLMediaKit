/*
 * Copyright (c) 2016-present The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/ZLMediaKit/ZLMediaKit).
 *
 * Use of this source code is governed by MIT-like license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#include "FlvPlayer.h"

using namespace std;
using namespace toolkit;

namespace mediakit {

FlvPlayer::FlvPlayer(const EventPoller::Ptr &poller) {
    setPoller(poller);
}

void FlvPlayer::play(const string &url) {
    TraceL << "play http-flv: " << url;
    _play_result = false;
    setProxyUrl((*this)[Client::kProxyUrl]);
    setHeaderTimeout((*this)[Client::kTimeoutMS].as<int>());
    setBodyTimeout((*this)[Client::kMediaTimeoutMS].as<int>());
    setMethod("GET");
    sendRequest(url);
}

void FlvPlayer::onResponseHeader(const string &status, const HttpClient::HttpHeader &header) {
    if (status != "200" && status != "206") {
        // http状态码不符合预期
        throw invalid_argument("bad http status code:" + status);
    }

    auto content_type = const_cast<HttpClient::HttpHeader &>(header)["Content-Type"];
    if (content_type.find("video/x-flv") != 0) {
        throw invalid_argument("content type not http-flv: " + content_type);
    }
}

void FlvPlayer::teardown() {
    HttpClientImp::shutdown();
}

void FlvPlayer::onResponseCompleted(const SockException &ex) {
    if (!_play_result) {
        _play_result = true;
        onPlayResult(ex);
    } else {
        onShutdown(ex);
    }
}

void FlvPlayer::onResponseBody(const char *buf, size_t size) {
    if (!_benchmark_mode) {
        // 性能测试模式不做数据解析，节省cpu
        FlvSplitter::input(buf, size);
    }
}

bool FlvPlayer::onRecvMetadata(const AMFValue &metadata) {
    return onMetadata(metadata);
}

void FlvPlayer::onRecvRtmpPacket(RtmpPacket::Ptr packet) {
    if (!_play_result && !packet->isConfigFrame()) {
        _play_result = true;
        _benchmark_mode = (*this)[Client::kBenchmarkMode].as<int>();
        onPlayResult(SockException(Err_success, "play http-flv success"));
    }
    onRtmpPacket(std::move(packet));
}

}//mediakit