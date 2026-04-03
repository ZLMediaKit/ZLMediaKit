/*
 * Copyright (c) 2016-present The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/ZLMediaKit/ZLMediaKit).
 *
 * Use of this source code is governed by MIT-like license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#ifndef ZLMEDIAKIT_WEBRTC_PROXY_PLAYER_IMP_H
#define ZLMEDIAKIT_WEBRTC_PROXY_PLAYER_IMP_H

#include "WebRtcProxyPlayer.h"

namespace mediakit {

class WebRtcProxyPlayerImp
    : public PlayerImp<WebRtcProxyPlayer, PlayerBase>
    , private TrackListener {
public:
    using Ptr = std::shared_ptr<WebRtcProxyPlayerImp>;
    using Super = PlayerImp<WebRtcProxyPlayer, PlayerBase>;

    WebRtcProxyPlayerImp(const toolkit::EventPoller::Ptr &poller) : Super(poller) {}
    ~WebRtcProxyPlayerImp() override { DebugL; }

private:

    //// WebRtcProxyPlayer override////
    void startConnect() override;

    //// PlayerBase override////
    void onResult(const toolkit::SockException &ex) override;
    std::vector<Track::Ptr> getTracks(bool ready = true) const override;

    //// TrackListener override////
    bool addTrack(const Track::Ptr &track) override { return true; }
    void addTrackCompleted() override;
};

} /* namespace mediakit */
#endif /* ZLMEDIAKIT_WEBRTC_PROXY_PLAYER_IMP_H */
