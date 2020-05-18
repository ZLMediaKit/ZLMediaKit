/*
 * Copyright (c) 2020 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/xiongziliang/ZLMediaKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#include "HttpTSPlayer.h"
namespace mediakit {

HttpTSPlayer::HttpTSPlayer(const EventPoller::Ptr &poller, bool split_ts){
    _segment.setOnSegment([this](const char *data, uint64_t len) { onPacket(data, len); });
    _poller = poller ? poller : EventPollerPool::Instance().getPoller();
    _split_ts = split_ts;
}

HttpTSPlayer::~HttpTSPlayer() {}

int64_t HttpTSPlayer::onResponseHeader(const string &status, const HttpClient::HttpHeader &headers) {
    if (status != "200" && status != "206") {
        //http状态码不符合预期
        shutdown(SockException(Err_other, StrPrinter << "bad http status code:" + status));
        return 0;
    }
    auto contet_type = const_cast< HttpClient::HttpHeader &>(headers)["Content-Type"];
    if (contet_type.find("video/mp2t") == 0 || contet_type.find("video/mpeg") == 0) {
        _is_ts_content = true;
    }

    //后续是不定长content
    return -1;
}

void HttpTSPlayer::onResponseBody(const char *buf, int64_t size, int64_t recvedSize, int64_t totalSize) {
    if (recvedSize == size) {
        //开始接收数据
        if (buf[0] == TS_SYNC_BYTE) {
            //这是ts头
            _is_first_packet_ts = true;
        } else {
            WarnL << "可能不是http-ts流";
        }
    }

    if (_split_ts) {
        _segment.input(buf, size);
    } else {
        onPacket(buf, size);
    }
}

void HttpTSPlayer::onResponseCompleted() {
    //接收完毕
    shutdown(SockException(Err_success, "play completed"));
}

void HttpTSPlayer::onDisconnect(const SockException &ex) {
    if (_on_disconnect) {
        _on_disconnect(ex);
        _on_disconnect = nullptr;
    }
}

void HttpTSPlayer::onPacket(const char *data, uint64_t len) {
    if (_on_segment) {
        _on_segment(data, len);
    }
}

void HttpTSPlayer::setOnDisconnect(const HttpTSPlayer::onShutdown &cb) {
    _on_disconnect = cb;
}

void HttpTSPlayer::setOnPacket(const TSSegment::onSegment &cb) {
    _on_segment = cb;
}

}//namespace mediakit