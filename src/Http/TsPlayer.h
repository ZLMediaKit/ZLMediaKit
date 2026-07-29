/*
 * Copyright (c) 2020 The ZLMediaKit project authors. All Rights Reserved.
 * Created by alex on 2021/4/6.
 * This file is part of ZLMediaKit(https://github.com/ZLMediaKit/ZLMediaKit).
 *
 * Use of this source code is governed by MIT-like license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#ifndef HTTP_TSPLAYER_H
#define HTTP_TSPLAYER_H

#include "HttpTSPlayer.h"
#include "Player/PlayerBase.h"

namespace mediakit {

class TsPlayer : public HttpTSPlayer, public PlayerBase {
public:
    TsPlayer(const toolkit::EventPoller::Ptr &poller);

    /**
     * 开始播放
     * Start playing
     
     * [AUTO-TRANSLATED:53a212c5]
     */
    void play(const std::string &url) override;

    size_t getRecvSpeed() override;
    size_t getRecvTotalBytes() override;

    /**
     * 停止播放
     * Stop playing
     
     
     * [AUTO-TRANSLATED:db52bf15]
     */
    void teardown() override;

protected:
    void onResponseBody(const char *buf, size_t size) override;
    void onResponseCompleted(const toolkit::SockException &ex) override;
    // 将 HTTP 完成状态转换为播放器最终的 shutdown 状态，确保所有完成回调语义一致。
    // Convert the HTTP completion status to the final player shutdown status so all completion callbacks agree.
    virtual toolkit::SockException translateShutdownException(const toolkit::SockException &ex) { return ex; }

private:
    bool _play_result = true;
    bool _benchmark_mode = false;
};

} // namespace mediakit
#endif // HTTP_TSPLAYER_H
