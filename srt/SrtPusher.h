/*
 * Copyright (c) 2016-present The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/ZLMediaKit/ZLMediaKit).
 *
 * Use of this source code is governed by MIT-like license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#ifndef ZLMEDIAKIT_SRTPUSHER_H
#define ZLMEDIAKIT_SRTPUSHER_H

#include "Network/Socket.h"
#include "Pusher/PusherBase.h"
#include "Poller/Timer.h"
#include "Util/TimeTicker.h"
#include "srt/SrtTransport.hpp"
#include "Http/HttpRequester.h"
#include <memory>
#include <string>
#include "SrtCaller.h"

namespace mediakit {

// 实现了srt代理推流功能
class SrtPusher
    : public PusherBase , public SrtCaller {
public:
    using Ptr = std::shared_ptr<SrtPusher>;

    SrtPusher(const toolkit::EventPoller::Ptr &poller,const TSMediaSource::Ptr &src);
    ~SrtPusher() override;

    //// PusherBase override////
    void publish(const std::string &url) override;
    void teardown() override;

    void doPublish();
protected:

    //// SrtCaller override////
    void onHandShakeFinished() override;
    void onResult(const toolkit::SockException &ex) override;

    bool isPlayer() override {return false;}
    uint16_t getLatency() override;
    float getTimeOutSec() override;
    std::string getPassphrase() override;

protected:
    std::weak_ptr<TSMediaSource> _push_src;
    TSMediaSource::RingType::RingReader::Ptr _ts_reader;

    size_t getSendSpeed() override;
    size_t getSendTotalBytes() override;
};

using SrtPusherImp = PusherImp<SrtPusher, PusherBase>;

} /* namespace mediakit */
#endif /* ZLMEDIAKIT_SRTPUSHER_H */
