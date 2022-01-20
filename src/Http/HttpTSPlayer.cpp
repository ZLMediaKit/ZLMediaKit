/*
 * Copyright (c) 2020 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/xia-chu/ZLMediaKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#include "HttpTSPlayer.h"

namespace mediakit {

HttpTSPlayer::HttpTSPlayer(const EventPoller::Ptr &poller, bool split_ts) {
    _split_ts = split_ts;
    _segment.setOnSegment([this](const char *data, size_t len) { onPacket(data, len); });
    setPoller(poller ? poller : EventPollerPool::Instance().getPoller());
}

void HttpTSPlayer::onResponseHeader(const string &status, const HttpClient::HttpHeader &header) {
    if (status != "200" && status != "206") {
        // http状态码不符合预期
        throw invalid_argument("bad http status code:" + status);
    }

    auto content_type = const_cast<HttpClient::HttpHeader &>(header)["Content-Type"];
    if (content_type.find("video/mp2t") != 0 && content_type.find("video/mpeg") != 0) {
        throw invalid_argument("content type not mpeg-ts: " + content_type);
    }
}

void HttpTSPlayer::onResponseBody(const char *buf, size_t size) {
    if (_split_ts) {
        try {
            _segment.input(buf, size);
        } catch (std::exception &ex) {
            WarnL << ex.what();
            // ts解析失败，清空缓存数据
            _segment.reset();
            throw;
        }
    } else {
        onPacket(buf, size);
    }
}

void HttpTSPlayer::onResponseCompleted(const SockException &ex) {
    emitOnComplete(ex);
}

void HttpTSPlayer::emitOnComplete(const SockException &ex) {
    if (_on_complete) {
        _on_complete(ex);
        _on_complete = nullptr;
    }
}

void HttpTSPlayer::onPacket(const char *data, size_t len) {
    if (_on_segment) {
        _on_segment(data, len);
    }
}

void HttpTSPlayer::setOnComplete(onComplete cb) {
    _on_complete = std::move(cb);
}

void HttpTSPlayer::setOnPacket(TSSegment::onSegment cb) {
    _on_segment = std::move(cb);
}

} // namespace mediakit
