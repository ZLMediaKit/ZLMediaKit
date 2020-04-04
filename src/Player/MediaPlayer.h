/*
 * Copyright (c) 2016 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/xiongziliang/ZLMediaKit).
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
using namespace toolkit;

namespace mediakit {

class MediaPlayer : public PlayerImp<PlayerBase,PlayerBase> {
public:
    typedef std::shared_ptr<MediaPlayer> Ptr;

    MediaPlayer(const EventPoller::Ptr &poller = nullptr);
    virtual ~MediaPlayer();
    void play(const string &strUrl) override;
    void pause(bool bPause) override;
    void teardown() override;
    EventPoller::Ptr getPoller();
private:
    EventPoller::Ptr _poller;
};

} /* namespace mediakit */

#endif /* SRC_PLAYER_MEDIAPLAYER_H_ */
