/*
 * Copyright (c) 2020 The ZLMediaKit project authors. All Rights Reserved.
 * Created by alex on 2021/4/6.
 * This file is part of ZLMediaKit(https://github.com/xia-chu/ZLMediaKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#include "TsPlayerImp.h"
#include "HlsPlayer.h"

using namespace std;
using namespace toolkit;

namespace mediakit {

TsPlayerImp::TsPlayerImp(const EventPoller::Ptr &poller) : PlayerImp<TsPlayer, PlayerBase>(poller) {}

void TsPlayerImp::onPacket(const char *data, size_t len) {
    if (!_decoder && _demuxer) {
        _decoder = DecoderImp::createDecoder(DecoderImp::decoder_ts, _demuxer.get());
    }

    if (_decoder && _demuxer) {
        _decoder->input((uint8_t *) data, len);
    }
}

void TsPlayerImp::addTrackCompleted() {
    PlayerImp<TsPlayer, PlayerBase>::onPlayResult(SockException(Err_success, "play http-ts success"));
}

void TsPlayerImp::onPlayResult(const SockException &ex) {
    if (ex) {
        PlayerImp<TsPlayer, PlayerBase>::onPlayResult(ex);
    } else {
        auto demuxer = std::make_shared<HlsDemuxer>();
        demuxer->start(getPoller(), this);
        _demuxer = std::move(demuxer);
    }
}

void TsPlayerImp::onShutdown(const SockException &ex) {
    PlayerImp<TsPlayer, PlayerBase>::onShutdown(ex);
    _demuxer = nullptr;
}

vector <Track::Ptr> TsPlayerImp::getTracks(bool ready) const {
    return static_pointer_cast<HlsDemuxer>(_demuxer)->getTracks(ready);
}

}//namespace mediakit