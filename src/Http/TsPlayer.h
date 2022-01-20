/*
 * Copyright (c) 2020 The ZLMediaKit project authors. All Rights Reserved.
 * Created by alex on 2021/4/6.
 * This file is part of ZLMediaKit(https://github.com/xia-chu/ZLMediaKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#ifndef HTTP_TSPLAYER_H
#define HTTP_TSPLAYER_H

#include "HttpTSPlayer.h"
#include "Player/PlayerBase.h"

using namespace toolkit;
namespace mediakit {

class TsPlayer : public HttpTSPlayer , public PlayerBase {
public:
    TsPlayer(const EventPoller::Ptr &poller);
    ~TsPlayer() override = default;

    /**
     * 开始播放
     */
    void play(const string &url) override;

    /**
     * 停止播放
     */
    void teardown() override;

protected:
    void onResponseBody(const char *buf, size_t size) override;
    void onResponseCompleted(const SockException &ex) override;

private:
    bool _play_result = true;
};

} // namespace mediakit
#endif // HTTP_TSPLAYER_H
