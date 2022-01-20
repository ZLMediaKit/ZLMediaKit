/*
 * Copyright (c) 2020 The ZLMediaKit project authors. All Rights Reserved.
 * Created by alex on 2021/4/6.
 * This file is part of ZLMediaKit(https://github.com/xia-chu/ZLMediaKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#ifndef HTTP_TSPLAYERIMP_H
#define HTTP_TSPLAYERIMP_H

#include <unordered_set>
#include "TsPlayer.h"

using namespace toolkit;

namespace mediakit {

class TsPlayerImp : public PlayerImp<TsPlayer, PlayerBase>, private TrackListener {
public:
    using Ptr = std::shared_ptr<TsPlayerImp>;

    TsPlayerImp(const EventPoller::Ptr &poller = nullptr);
    ~TsPlayerImp() override = default;

private:
    //// HlsPlayer override////
    void onPacket(const char *data, size_t len) override;

private:
    //// PlayerBase override////
    void onPlayResult(const SockException &ex) override;
    vector<Track::Ptr> getTracks(bool ready = true) const override;
    void onShutdown(const SockException &ex) override;

private:
    //// TrackListener override////
    bool addTrack(const Track::Ptr &track) override { return true; };
    void addTrackCompleted() override;

private:
    DecoderImp::Ptr _decoder;
    MediaSinkInterface::Ptr _demuxer;
};

}//namespace mediakit
#endif //HTTP_TSPLAYERIMP_H
