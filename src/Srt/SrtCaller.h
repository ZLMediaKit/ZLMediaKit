/*
 * Copyright (c) 2016-present The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/ZLMediaKit/ZLMediaKit).
 *
 * Use of this source code is governed by MIT-like license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#ifndef ZLMEDIAKIT_SRTCALLER_H
#define ZLMEDIAKIT_SRTCALLER_H

//srt
#include "srt/Packet.hpp"
#include "srt/Crypto.hpp"
#include "srt/PacketQueue.hpp"
#include "srt/PacketSendQueue.hpp"
#include "srt/Statistic.hpp"

#include "Poller/EventPoller.h"
#include "Network/Socket.h"
#include "Poller/Timer.h"
#include "Util/TimeTicker.h"
#include "Common/MultiMediaSourceMuxer.h"
#include "Rtp/Decoder.h"
#include "TS/TSMediaSource.h"
#include <memory>
#include <string>


namespace mediakit {

// 解析srt 信令url的工具类
class SrtUrl {
public:
    std::string _full_url;
    std::string _params;
    std::string _host;
    uint16_t _port;
    std::string _streamid;

public:
    void parse(const std::string &url);
};

// 实现了webrtc代理拉流功能
class SrtCaller : public std::enable_shared_from_this<SrtCaller>{
public:
    using Ptr = std::shared_ptr<SrtCaller>;

    using SteadyClock = std::chrono::steady_clock;
    using TimePoint = std::chrono::time_point<SteadyClock>;

    SrtCaller(const toolkit::EventPoller::Ptr &poller);
    virtual ~SrtCaller();

    const toolkit::EventPoller::Ptr &getPoller() const {return _poller;}

    virtual void inputSockData(uint8_t *buf, int len, struct sockaddr *addr);
    virtual void onSendTSData(const SRT::Buffer::Ptr &buffer, bool flush);

    size_t getRecvSpeed() const;
    size_t getRecvTotalBytes() const;
    size_t getSendSpeed() const;
    size_t getSendTotalBytes() const;

protected:

    virtual void onConnect();
    virtual void onHandShakeFinished();
    virtual void onResult(const toolkit::SockException &ex);

    virtual void onSRTData(SRT::DataPacket::Ptr pkt);

    virtual uint16_t getLatency() = 0;
    virtual int getLatencyMul();
    virtual int getPktBufSize();
    virtual float getTimeOutSec();

    virtual bool isPlayer() = 0;

private:
    void doHandshake();

    void sendHandshakeInduction();
    void sendHandshakeConclusion();
    void sendACKPacket();
    void sendLightACKPacket();
    void sendNAKPacket(std::list<SRT::PacketQueue::LostPair> &lost_list);
    void sendMsgDropReq(uint32_t first, uint32_t last);
    void sendKeepLivePacket();
    void sendShutDown();
    void tryAnnounceKeyMaterial();
    void sendControlPacket(SRT::ControlPacket::Ptr pkt, bool flush = true);
    void sendDataPacket(SRT::DataPacket::Ptr pkt, char *buf, int len, bool flush = false);
    void sendPacket(toolkit::Buffer::Ptr pkt, bool flush);

    void handleHandshake(uint8_t *buf, int len, struct sockaddr *addr);
    void handleHandshakeInduction(SRT::HandshakePacket &pkt, struct sockaddr *addr);
    void handleHandshakeConclusion(SRT::HandshakePacket &pkt, struct sockaddr *addr);
    void handleACK(uint8_t *buf, int len, struct sockaddr *addr);
    void handleACKACK(uint8_t *buf, int len, struct sockaddr *addr);
    void handleNAK(uint8_t *buf, int len, struct sockaddr *addr);
    void handleDropReq(uint8_t *buf, int len, struct sockaddr *addr);
    void handleKeeplive(uint8_t *buf, int len, struct sockaddr *addr);
    void handleShutDown(uint8_t *buf, int len, struct sockaddr *addr);
    void handlePeerError(uint8_t *buf, int len, struct sockaddr *addr);
    void handleCongestionWarning(uint8_t *buf, int len, struct sockaddr *addr);
    void handleUserDefinedType(uint8_t *buf, int len, struct sockaddr *addr);
    void handleDataPacket(uint8_t *buf, int len, struct sockaddr *addr);
    void handleKeyMaterialReqPacket(uint8_t *buf, int len, struct sockaddr *addr);
    void handleKeyMaterialRspPacket(uint8_t *buf, int len, struct sockaddr *addr);

    void checkAndSendAckNak();
    void createTimerForCheckAlive();

    std::string generateStreamId();
    uint32_t generateSocketId();
    int32_t generateInitSeq();
    size_t  getPayloadSize();

    virtual std::string getPassphrase() = 0;

protected:
    SrtUrl _url;
    toolkit::EventPoller::Ptr _poller;

    bool _is_handleshake_finished = false;

private:
    toolkit::Socket::Ptr _socket;

    TimePoint _now;
    TimePoint _start_timestamp;
    // for calculate rtt for delay
    TimePoint _induction_ts;

    //the initial value of RTT is 100 milliseconds
    //RTTVar is 50 milliseconds
    uint32_t _rtt          = 100 * 1000;
    uint32_t _rtt_variance = 50 * 1000;

    //local
    uint32_t _socket_id            = 0;
    uint32_t _init_seq_number       = 0;
    uint32_t _mtu                  = 1500;
    uint32_t _max_flow_window_size = 8192;
    uint16_t _delay                = 120;

    //peer
    uint32_t _sync_cookie          = 0;
    uint32_t _peer_socket_id;

    // for handshake
    SRT::Timer::Ptr _handleshake_timer;
    SRT::HandshakePacket::Ptr _handleshake_req;

    // for keeplive 
    SRT::Ticker _send_ticker;
    SRT::Timer::Ptr _keeplive_timer;

    // for alive
    SRT::Ticker _alive_ticker;
    SRT::Timer::Ptr _alive_timer;

    // for recv
    SRT::PacketQueueInterface::Ptr _recv_buf;
    uint32_t _last_pkt_seq = 0;

    // Ack
    SRT::UTicker _ack_ticker;
    uint32_t _last_ack_pkt_seq    = 0;
    uint32_t _light_ack_pkt_count = 0;
    uint32_t _ack_number_count    = 0;
    std::map<uint32_t, TimePoint> _ack_send_timestamp;
    // Full Ack
    // Link Capacity and Receiving Rate Estimation
    std::shared_ptr<SRT::PacketRecvRateContext> _pkt_recv_rate_context;
    std::shared_ptr<SRT::EstimatedLinkCapacityContext> _estimated_link_capacity_context;

    // Nak
    SRT::UTicker _nak_ticker;

    //for Send
    SRT::PacketSendQueue::Ptr _send_buf;
    SRT::ResourcePool<SRT::BufferRaw> _packet_pool;
    uint32_t _send_packet_seq_number = 0;
    uint32_t _send_msg_number        = 1;

    //AckAck
    uint32_t _last_recv_ackack_seq_num = 0;

    // for encryption
    SRT::Crypto::Ptr _crypto;
    SRT::Timer::Ptr _announce_timer;
    SRT::KeyMaterialPacket::Ptr _announce_req;
};

} /* namespace mediakit */
#endif /* ZLMEDIAKIT_SRTCALLER_H */

