/*
 * Copyright (c) 2020 The ZLMediaKit project authors. All Rights Reserved.
 * Created by alex on 2021/4/6.
 * This file is part of ZLMediaKit(https://github.com/ZLMediaKit/ZLMediaKit).
 *
 * Use of this source code is governed by MIT-like license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#include "TsPlayerImp.h"
#include "HlsPlayer.h"
#include "Common/config.h"

using namespace std;
using namespace toolkit;

namespace mediakit {

TsPlayerImp::TsPlayerImp(const EventPoller::Ptr &poller) : PlayerImp<TsPlayer, PlayerBase>(poller) {}

void TsPlayerImp::onResponseBody(const char *data, size_t len) {
    TsPlayer::onResponseBody(data, len);
    if (!_decoder && _demuxer) {
        _decoder = DecoderImp::createDecoder(DecoderImp::decoder_ts, _demuxer.get());
    }

    if (_decoder && _demuxer) {
        _decoder->input((uint8_t *) data, len);
    }
}

void TsPlayerImp::onManager() {
    HttpClientImp::onManager();
    if (!waitResponse() || !_demuxer) {
        return;
    }

    auto frame_count = _demuxer->getInputFrameCount();
    if (frame_count != _last_input_frame_count) {
        _last_input_frame_count = frame_count;
        _media_frame_ticker.resetTime();
        return;
    }

    auto timeout_ms = getEffectiveBodyTimeout();
    if (timeout_ms > 0 && _media_frame_ticker.elapsedTime() > timeout_ms) {
        _media_frame_timeout = true;
        shutdown(SockException(Err_timeout, "http-ts media frame timeout"));
    }
}

void TsPlayerImp::addTrackCompleted() {
    PlayerImp<TsPlayer, PlayerBase>::onPlayResult(SockException(Err_success, "play http-ts success"));
}

void TsPlayerImp::onPlayResult(const SockException &ex) {
    auto benchmark_mode = (*this)[Client::kBenchmarkMode].as<int>();
    if (ex || benchmark_mode) {
        PlayerImp<TsPlayer, PlayerBase>::onPlayResult(ex);
    } else {
        auto demuxer = std::make_shared<HlsDemuxer>();
        demuxer->start(getPoller(), this);
        _demuxer = std::move(demuxer);
        resetMediaFrameCheck();
    }
}

void TsPlayerImp::onShutdown(const SockException &ex) {
    auto shutdown_ex = ex;
    if (_media_frame_timeout) {
        // 无 Content-Length 的 HTTP body 在已收到数据后会把断开归一化为成功，
        // 因此需要在 player 层恢复本次主动触发的媒体超时语义。
        // An HTTP body without Content-Length normalizes a close after data to success,
        // so restore the media timeout explicitly triggered by this player.
        _media_frame_timeout = false;
        shutdown_ex.reset(Err_timeout, "http-ts media frame timeout");
    } else if (!shutdown_ex) {
        // http-ts拉流，如果为eof正常断开，那么强制为异常状态
        shutdown_ex.reset(Err_other, ex.what());
    }
    while (_demuxer) {
        try {
            // shared_from_this()可能抛异常  [AUTO-TRANSLATED:6af9bd3c]
            // shared_from_this() may throw an exception
            std::weak_ptr<TsPlayerImp> weak_self = static_pointer_cast<TsPlayerImp>(shared_from_this());
            if (_decoder) {
                _decoder->flush();
            }
            // 等待所有frame flush输出后，再触发onShutdown事件  [AUTO-TRANSLATED:93982eb3]
            // Wait for all frame flush output before triggering the onShutdown event
            _demuxer->pushTask([weak_self, shutdown_ex]() {
                if (auto strong_self = weak_self.lock()) {
                    strong_self->_demuxer = nullptr;
                    strong_self->onShutdown(shutdown_ex);
                }
            });
            return;
        } catch (...) {
            break;
        }
    }
    PlayerImp<TsPlayer, PlayerBase>::onShutdown(shutdown_ex);
}

void TsPlayerImp::resetMediaFrameCheck() {
    _last_input_frame_count = _demuxer ? _demuxer->getInputFrameCount() : 0;
    _media_frame_ticker.resetTime();
}

vector<Track::Ptr> TsPlayerImp::getTracks(bool ready) const {
    if (!_demuxer) {
        return vector<Track::Ptr>();
    }
    return _demuxer->getTracks(ready);
}

size_t TsPlayerImp::getRecvSpeed() {
    return TcpClient::getRecvSpeed();
}

size_t TsPlayerImp::getRecvTotalBytes() {
    return TcpClient::getRecvTotalBytes();
}
}//namespace mediakit
