/*
 * Copyright (c) 2016-present The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/ZLMediaKit/ZLMediaKit).
 *
 * Use of this source code is governed by MIT-like license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#ifndef ZLMEDIAKIT_SRTPLAYER_H
#define ZLMEDIAKIT_SRTPLAYER_H

#include "Network/Socket.h"
#include "Player/PlayerBase.h"
#include "Poller/Timer.h"
#include "Util/TimeTicker.h"
#include "srt/SrtTransport.hpp"
#include "Http/HttpRequester.h"
#include <memory>
#include <string>
#include "SrtCaller.h"

namespace mediakit {


// 实现了srt代理拉流功能
class SrtPlayer
    : public PlayerBase , public SrtCaller {
public:
    using Ptr = std::shared_ptr<SrtPlayer>;

    SrtPlayer(const toolkit::EventPoller::Ptr &poller);
    ~SrtPlayer() override;

    //// PlayerBase override////
    void play(const std::string &strUrl) override;
    void teardown() override;
    void pause(bool pause) override;
    void speed(float speed) override;
    size_t getRecvSpeed() override;
    size_t getRecvTotalBytes() override;

protected:

    //// SrtCaller override////
    void onHandShakeFinished() override;
    void onSRTData(SRT::DataPacket::Ptr pkt) override;
    void onResult(const toolkit::SockException &ex) override;

    bool isPlayer() override {return true;}

    uint16_t getLatency() override;
    float getTimeOutSec() override;
    std::string getPassphrase() override;

protected:
    //是否为性能测试模式
    bool _benchmark_mode = false;

    //超时功能实现
    toolkit::Ticker _recv_ticker;
    std::shared_ptr<toolkit::Timer> _check_timer;
};

} /* namespace mediakit */
#endif /* ZLMEDIAKIT_SRTPLAYER_H */
