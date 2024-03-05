/*
 * Copyright (c) 2016-present The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/ZLMediaKit/ZLMediaKit).
 *
 * Use of this source code is governed by MIT-like license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#ifndef ZLMEDIAKIT_RTPSESSION_H
#define ZLMEDIAKIT_RTPSESSION_H

#if defined(ENABLE_RTPPROXY)

#include "Network/Session.h"
#include "RtpSplitter.h"
#include "RtpProcess.h"
#include "Util/TimeTicker.h"

namespace mediakit{

class RtpSession : public toolkit::Session, public RtpSplitter, public MediaSourceEvent {
public:
    static const std::string kStreamID;
    static const std::string kSSRC;
    static const std::string kOnlyTrack;
    static const std::string kUdpRecvBuffer;

    RtpSession(const toolkit::Socket::Ptr &sock);
    ~RtpSession() override;
    void onRecv(const toolkit::Buffer::Ptr &) override;
    void onError(const toolkit::SockException &err) override;
    void onManager() override;
    void setParams(toolkit::mINI &ini);
    void attachServer(const toolkit::Server &server) override;

protected:
    // 通知其停止推流
    bool close(MediaSource &sender) override;
    // 收到rtp回调
    void onRtpPacket(const char *data, size_t len) override;
    // RtpSplitter override
    const char *onSearchPacketTail(const char *data, size_t len) override;
    // 搜寻SSRC
    const char *searchBySSRC(const char *data, size_t len);
    // 搜寻PS包里的关键帧标头
    const char *searchByPsHeaderFlag(const char *data, size_t len);

private:
    bool _delay_close = false;
    bool _is_udp = false;
    bool _search_rtp = false;
    bool _search_rtp_finished = false;
    int _only_track = 0;
    uint32_t _ssrc = 0;
    toolkit::Ticker _ticker;
    std::string _stream_id;
    struct sockaddr_storage _addr;
    RtpProcess::Ptr _process;
};

}//namespace mediakit
#endif//defined(ENABLE_RTPPROXY)
#endif //ZLMEDIAKIT_RTPSESSION_H
