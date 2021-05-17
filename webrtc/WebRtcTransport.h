/*
 * Copyright (c) 2016 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/xia-chu/ZLMediaKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#pragma once

#include <memory>
#include <string>
#include "DtlsTransport.hpp"
#include "IceServer.hpp"
#include "SrtpSession.hpp"
#include "StunPacket.hpp"
#include "Sdp.h"
#include "Poller/EventPoller.h"
#include "Network/Socket.h"
#include "Rtsp/RtspMediaSourceImp.h"
#include "Rtcp/RtcpContext.h"
#include "Rtcp/RtcpFCI.h"
using namespace toolkit;
using namespace mediakit;

class WebRtcTransport : public RTC::DtlsTransport::Listener, public RTC::IceServer::Listener  {
public:
    using Ptr = std::shared_ptr<WebRtcTransport>;
    WebRtcTransport(const EventPoller::Ptr &poller);
    ~WebRtcTransport() override = default;

    /**
     * 创建对象
     */
    virtual void onCreate();

    /**
     * 销毁对象
     */
    virtual void onDestory();

    /**
     * 创建webrtc answer sdp
     * @param offer offer sdp
     * @return answer sdp
     */
    std::string getAnswerSdp(const string &offer);

    /**
     * socket收到udp数据
     * @param buf 数据指针
     * @param len 数据长度
     * @param tuple 数据来源
     */
    void inputSockData(char *buf, size_t len, RTC::TransportTuple *tuple);

    /**
     * 发送rtp
     * @param buf rtcp内容
     * @param len rtcp长度
     * @param flush 是否flush socket
     * @param ctx 用户指针
     */
    void sendRtpPacket(const char *buf, size_t len, bool flush, void *ctx = nullptr);
    void sendRtcpPacket(const char *buf, size_t len, bool flush, void *ctx = nullptr);

    const EventPoller::Ptr& getPoller() const;

protected:
    ////  dtls相关的回调 ////
    void OnDtlsTransportConnecting(const RTC::DtlsTransport *dtlsTransport) override;
    void OnDtlsTransportConnected(const RTC::DtlsTransport *dtlsTransport,
                                  RTC::SrtpSession::CryptoSuite srtpCryptoSuite,
                                  uint8_t *srtpLocalKey,
                                  size_t srtpLocalKeyLen,
                                  uint8_t *srtpRemoteKey,
                                  size_t srtpRemoteKeyLen,
                                  std::string &remoteCert) override;

    void OnDtlsTransportFailed(const RTC::DtlsTransport *dtlsTransport) override;
    void OnDtlsTransportClosed(const RTC::DtlsTransport *dtlsTransport) override;
    void OnDtlsTransportSendData(const RTC::DtlsTransport *dtlsTransport, const uint8_t *data, size_t len) override;
    void OnDtlsTransportApplicationDataReceived(const RTC::DtlsTransport *dtlsTransport, const uint8_t *data, size_t len) override;

protected:
    //// ice相关的回调 ///
    void OnIceServerSendStunPacket(const RTC::IceServer *iceServer, const RTC::StunPacket *packet, RTC::TransportTuple *tuple) override;
    void OnIceServerSelectedTuple(const RTC::IceServer *iceServer, RTC::TransportTuple *tuple) override;
    void OnIceServerConnected(const RTC::IceServer *iceServer) override;
    void OnIceServerCompleted(const RTC::IceServer *iceServer) override;
    void OnIceServerDisconnected(const RTC::IceServer *iceServer) override;

protected:
    virtual void onStartWebRTC() = 0;
    virtual void onRtcConfigure(RtcConfigure &configure) const;
    virtual void onCheckSdp(SdpType type, RtcSession &sdp);
    virtual void onSendSockData(const char *buf, size_t len, struct sockaddr_in *dst, bool flush = true) = 0;

    virtual void onRtp(const char *buf, size_t len) = 0;
    virtual void onRtcp(const char *buf, size_t len) = 0;
    virtual void onShutdown(const SockException &ex) = 0;
    virtual void onBeforeEncryptRtp(const char *buf, size_t &len, void *ctx) = 0;
    virtual void onBeforeEncryptRtcp(const char *buf, size_t &len, void *ctx) = 0;

protected:
    const RtcSession& getSdp(SdpType type) const;
    RTC::TransportTuple* getSelectedTuple() const;
    void sendRtcpRemb(uint32_t ssrc, size_t bit_rate);
    void sendRtcpPli(uint32_t ssrc);

private:
    void onSendSockData(const char *buf, size_t len, bool flush = true);
    void setRemoteDtlsFingerprint(const RtcSession &remote);

private:
    uint8_t _srtp_buf[2000];
    EventPoller::Ptr _poller;
    std::shared_ptr<RTC::IceServer> _ice_server;
    std::shared_ptr<RTC::DtlsTransport> _dtls_transport;
    std::shared_ptr<RTC::SrtpSession> _srtp_session_send;
    std::shared_ptr<RTC::SrtpSession> _srtp_session_recv;
    RtcSession::Ptr _offer_sdp;
    RtcSession::Ptr _answer_sdp;
};

class RtpReceiverImp;

class NackList {
public:
    void push_back(RtpPacket::Ptr rtp) {
        auto seq = rtp->getSeq();
        _nack_cache_seq.emplace_back(seq);
        _nack_cache_pkt.emplace(seq, std::move(rtp));
        while (get_cache_ms() > kMaxNackMS) {
            //需要清除部分nack缓存
            pop_front();
        }
    }

    template<typename FUNC>
    void for_each_nack(const FCI_NACK &nack, const FUNC &func) {
        auto seq = nack.getPid();
        for (auto bit : nack.getBitArray()) {
            if (bit) {
                //丢包
                RtpPacket::Ptr *ptr = get_rtp(seq);
                if (ptr) {
                    func(*ptr);
                }
            }
            ++seq;
        }
    }

private:
    void pop_front() {
        if (_nack_cache_seq.empty()) {
            return;
        }
        _nack_cache_pkt.erase(_nack_cache_seq.front());
        _nack_cache_seq.pop_front();
    }

    RtpPacket::Ptr *get_rtp(uint16_t seq) {
        auto it = _nack_cache_pkt.find(seq);
        if (it == _nack_cache_pkt.end()) {
            return nullptr;
        }
        return &it->second;
    }

    uint32_t get_cache_ms() {
        if (_nack_cache_seq.size() < 2) {
            return 0;
        }
        uint32_t back = _nack_cache_pkt[_nack_cache_seq.back()]->getStampMS();
        uint32_t front = _nack_cache_pkt[_nack_cache_seq.front()]->getStampMS();
        if (back > front) {
            return back - front;
        }
        //很有可能回环了
        return back + (UINT32_MAX - front);
    }

private:
    static constexpr uint32_t kMaxNackMS = 10 * 1000;
    deque<uint16_t> _nack_cache_seq;
    unordered_map<uint16_t, RtpPacket::Ptr > _nack_cache_pkt;
};

class NackContext {
public:
    using onNack = function<void(const FCI_NACK &nack)>;

    void received(uint16_t seq) {
        if (!_last_max_seq && _seq.empty()) {
            _last_max_seq = seq - 1;
        }
        _seq.emplace(seq);
        auto max_seq = *_seq.rbegin();
        auto min_seq = *_seq.begin();
        auto diff = max_seq - min_seq;
        if (!diff) {
            return;
        }

        if (diff > UINT32_MAX / 2) {
            //回环
            _seq.clear();
            _last_max_seq = min_seq;
            return;
        }

        if (_seq.size() == diff + 1 && _last_max_seq + 1 == min_seq) {
            //都是连续的seq，未丢包
            _seq.clear();
            _last_max_seq = max_seq;
        } else {
            //seq不连续，有丢包
            if (min_seq == _last_max_seq + 1) {
                //前面部分seq是连续的，未丢包，移除之
                eraseFrontSeq();
            }

            //有丢包，丢包从_last_max_seq开始
            if (max_seq - _last_max_seq > FCI_NACK::kBitSize) {
                vector<bool> vec;
                vec.resize(FCI_NACK::kBitSize);
                for (auto i = 0; i < FCI_NACK::kBitSize; ++i) {
                    vec[i] = _seq.find(_last_max_seq + i + 2) == _seq.end();
                }
                doNack(FCI_NACK(_last_max_seq + 1, vec));
                _last_max_seq += FCI_NACK::kBitSize + 1;
                if (_last_max_seq >= max_seq) {
                    _seq.clear();
                } else {
                    auto it = _seq.emplace_hint(_seq.begin(), _last_max_seq);
                    _seq.erase(_seq.begin(), it);
                }
            }
        }
    }

    void setOnNack(onNack cb) {
        _cb = std::move(cb);
    }

private:
    void doNack(const FCI_NACK &nack) {
        if (_cb) {
            _cb(nack);
        }
    }

    void eraseFrontSeq(){
        //前面部分seq是连续的，未丢包，移除之
        for (auto it = _seq.begin(); it != _seq.end();) {
            if (*it != _last_max_seq + 1) {
                //seq不连续，丢包了
                break;
            }
            _last_max_seq = *it;
            it = _seq.erase(it);
        }
    }

private:
    onNack _cb;
    set<uint16_t> _seq;
    uint16_t _last_max_seq = 0;
};

class WebRtcTransportImp : public WebRtcTransport, public MediaSourceEvent, public SockInfo, public std::enable_shared_from_this<WebRtcTransportImp>{
public:
    using Ptr = std::shared_ptr<WebRtcTransportImp>;
    ~WebRtcTransportImp() override;

    /**
     * 创建WebRTC对象
     * @param poller 改对象需要绑定的线程
     * @return 对象
     */
    static Ptr create(const EventPoller::Ptr &poller);

    /**
     * 绑定rtsp媒体源
     * @param src 媒体源
     * @param is_play 是播放还是推流
     */
    void attach(const RtspMediaSource::Ptr &src, const MediaInfo &info, bool is_play = true);

protected:
    void onStartWebRTC() override;
    void onSendSockData(const char *buf, size_t len, struct sockaddr_in *dst, bool flush = true) override;
    void onCheckSdp(SdpType type, RtcSession &sdp) override;
    void onRtcConfigure(RtcConfigure &configure) const override;

    void onRtp(const char *buf, size_t len) override;
    void onRtp_l(const char *buf, size_t len, bool rtx);

    void onRtcp(const char *buf, size_t len) override;
    void onBeforeEncryptRtp(const char *buf, size_t &len, void *ctx) override;
    void onBeforeEncryptRtcp(const char *buf, size_t &len, void *ctx) override {};

    void onShutdown(const SockException &ex) override;

    ///////MediaSourceEvent override///////
    // 关闭
    bool close(MediaSource &sender, bool force) override;
    // 播放总人数
    int totalReaderCount(MediaSource &sender) override;
    // 获取媒体源类型
    MediaOriginType getOriginType(MediaSource &sender) const override;
    // 获取媒体源url或者文件路径
    string getOriginUrl(MediaSource &sender) const override;
    // 获取媒体源客户端相关信息
    std::shared_ptr<SockInfo> getOriginSock(MediaSource &sender) const override;

    ///////SockInfo override///////
    //获取本机ip
    string get_local_ip() override;
    //获取本机端口号
    uint16_t get_local_port() override;
    //获取对方ip
    string get_peer_ip() override;
    //获取对方端口号
    uint16_t get_peer_port() override;
    //获取标识符
    string getIdentifier() const override;

private:
    WebRtcTransportImp(const EventPoller::Ptr &poller);
    void onCreate() override;
    void onDestory() override;
    void onSendRtp(const RtpPacket::Ptr &rtp, bool flush, bool rtx = false);
    SdpAttrCandidate::Ptr getIceCandidate() const;
    bool canSendRtp() const;
    bool canRecvRtp() const;

    class RtpPayloadInfo {
    public:
        using Ptr = std::shared_ptr<RtpPayloadInfo>;
        const RtcCodecPlan *plan_rtp;
        const RtcCodecPlan *plan_rtx;
        uint32_t offer_ssrc_rtp = 0;
        uint32_t offer_ssrc_rtx = 0;
        uint32_t answer_ssrc_rtp = 0;
        uint32_t answer_ssrc_rtx = 0;
        const RtcMedia *media;
        NackList nack_list;
        NackContext nack_ctx;
        RtcpContext::Ptr rtcp_context_recv;
        RtcpContext::Ptr rtcp_context_send;
        std::shared_ptr<RtpReceiverImp> receiver;
    };

    void onSortedRtp(RtpPayloadInfo &info, RtpPacket::Ptr rtp);
    void onSendNack(RtpPayloadInfo &info, const FCI_NACK &nack);
    void changeRtpExtId(const RtpPayloadInfo *info, const RtpHeader *header, bool is_recv, bool is_rtx = false) const;

private:
    uint16_t _rtx_seq[2] = {0, 0};
    //用掉的总流量
    uint64_t _bytes_usage = 0;
    //媒体相关元数据
    MediaInfo _media_info;
    //保持自我强引用
    Ptr _self;
    //检测超时的定时器
    Timer::Ptr _timer;
    //刷新计时器
    Ticker _alive_ticker;
    //pli rtcp计时器
    Ticker _pli_ticker;
    //复合udp端口，接收一切rtp与rtcp
    Socket::Ptr _socket;
    //推流的rtsp源
    RtspMediaSource::Ptr _push_src;
    //播放的rtsp源
    RtspMediaSource::Ptr _play_src;
    //播放rtsp源的reader对象
    RtspMediaSource::RingType::RingReader::Ptr _reader;
    //根据发送rtp的track类型获取相关信息
    RtpPayloadInfo::Ptr _send_rtp_info[2];
    //根据接收rtp的pt获取相关信息
    unordered_map<uint8_t/*pt*/, std::pair<bool/*is rtx*/,RtpPayloadInfo::Ptr> > _rtp_info_pt;
    //根据rtcp的ssrc获取相关信息
    unordered_map<uint32_t/*ssrc*/, std::pair<bool/*is rtx*/,RtpPayloadInfo::Ptr> > _rtp_info_ssrc;
    //发送rtp时需要修改rtp ext id
    map<RtpExtType, uint8_t> _rtp_ext_type_to_id;
    //接收rtp时需要修改rtp ext id
    unordered_map<uint8_t, RtpExtType> _rtp_ext_id_to_type;
};
