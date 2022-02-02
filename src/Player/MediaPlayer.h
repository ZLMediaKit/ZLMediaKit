/*
 * Copyright (c) 2016 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/xia-chu/ZLMediaKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#ifndef SRC_PLAYER_MEDIAPLAYER_H_
#define SRC_PLAYER_MEDIAPLAYER_H_

#include <memory>
#include <string>
#include "PlayerBase.h"
#include "Rtsp/RtspPlayer.h"
#include "Rtmp/RtmpPlayer.h"
#include "Thread/TaskExecutor.h"

namespace mediakit {

class MediaPlayer : public PlayerImp<PlayerBase, PlayerBase> {
public:
    using Ptr = std::shared_ptr<MediaPlayer>;

    MediaPlayer(const toolkit::EventPoller::Ptr &poller = nullptr);
    ~MediaPlayer() override = default;

    void play(const std::string &url) override;
    toolkit::EventPoller::Ptr getPoller();
    void setOnCreateSocket(toolkit::Socket::onCreateSocket cb);

private:
    toolkit::EventPoller::Ptr _poller;
    toolkit::Socket::onCreateSocket _on_create_socket;
};

} /* namespace mediakit */

#endif /* SRC_PLAYER_MEDIAPLAYER_H_ */
