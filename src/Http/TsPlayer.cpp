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

namespace mediakit {

TsPlayer::TsPlayer(const EventPoller::Ptr &poller) : HttpTSPlayer(poller, true) {}

void TsPlayer::play(const string &strUrl) {
    _ts_url.append(strUrl);
    playTs();
}

void TsPlayer::teardown() {
    shutdown(SockException(Err_shutdown, "teardown"));
}

void TsPlayer::playTs() {
    if (waitResponse()) {
        //播放器目前还存活，正在下载中
        return;
    }
    TraceL << "play http-ts: " << _ts_url;
    weak_ptr <TsPlayer> weak_self = dynamic_pointer_cast<TsPlayer>(shared_from_this());
    setMethod("GET");
    setHeaderTimeout((*this)[Client::kTimeoutMS].as<int>());
    setBodyTimeout((*this)[Client::kMediaTimeoutMS].as<int>());
    sendRequest(_ts_url);
}

void TsPlayer::onResponseCompleted() {
    //接收完毕
    shutdown(SockException(Err_success, StrPrinter << "play " << _ts_url << " completed"));
}

void TsPlayer::onDisconnect(const SockException &ex) {
    WarnL << "play " << _ts_url << " failed: " << ex.getErrCode() << " " << ex.what();
    if (_first) {
        //第一次失败，则播放失败
        _first = false;
        onPlayResult(ex);
        return;
    }
    if (ex.getErrCode() == Err_shutdown) {
        onShutdown(ex);
    } else {
        onResponseCompleted();
        onShutdown(ex);
    }
}

ssize_t TsPlayer::onResponseHeader(const string &status, const HttpClient::HttpHeader &header) {
    ssize_t ret = HttpTSPlayer::onResponseHeader(status, header);
    if (_first) {
        _first = false;
        onPlayResult(SockException(Err_success, "play success"));
    }
    return ret;
}

}//namespace mediakit