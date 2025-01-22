/*
 * Copyright (c) 2016-present The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/ZLMediaKit/ZLMediaKit).
 *
 * Use of this source code is governed by MIT-like license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#ifndef SRC_RTSPPLAYER_RTSPPLAYER_H_TXT_
#define SRC_RTSPPLAYER_RTSPPLAYER_H_TXT_

#include <string>
#include <memory>
#include "Util/TimeTicker.h"
#include "Poller/Timer.h"
#include "Network/Socket.h"
#include "Player/PlayerBase.h"
#include "Network/TcpClient.h"
#include "RtspSplitter.h"
#include "RtpReceiver.h"
#include "Rtcp/RtcpContext.h"

namespace mediakit {

// 实现了rtsp播放器协议部分的功能，及数据接收功能  [AUTO-TRANSLATED:c1ed5c0f]
// Implemented the rtsp player protocol part functionality, and data receiving functionality
class RtspPlayer : public PlayerBase, public toolkit::TcpClient, public RtspSplitter, public RtpReceiver {
public:
    using Ptr = std::shared_ptr<RtspPlayer>;

    RtspPlayer(const toolkit::EventPoller::Ptr &poller);
    ~RtspPlayer() override;

    void play(const std::string &strUrl) override;
    void pause(bool pause) override;
    void speed(float speed) override;
    void teardown() override;
    float getPacketLossRate(TrackType type) const override;

protected:
    // 派生类回调函数  [AUTO-TRANSLATED:61e20903]
    // Derived class callback function
    virtual bool onCheckSDP(const std::string &sdp) = 0;
    virtual void onRecvRTP(RtpPacket::Ptr rtp, const SdpTrack::Ptr &track) = 0;
    uint32_t getProgressMilliSecond() const;
    void seekToMilliSecond(uint32_t ms);

    /**
     * 收到完整的rtsp包回调，包括sdp等content数据
     * @param parser rtsp包
     * Callback for receiving a complete rtsp packet, including sdp and other content data
     * @param parser rtsp packet
     
     * [AUTO-TRANSLATED:4d3c2056]
     */
    void onWholeRtspPacket(Parser &parser) override ;

    /**
     * 收到rtp包回调
     * @param data
     * @param len
     * Callback for receiving rtp packet
     * @param data
     * @param len
     
     * [AUTO-TRANSLATED:c8f7c9bb]
     */
    void onRtpPacket(const char *data,size_t len) override ;

    /**
     * rtp数据包排序后输出
     * @param rtp rtp数据包
     * @param track_idx track索引
     * Output rtp data packets after sorting
     * @param rtp rtp data packet
     * @param track_idx track index
     
     * [AUTO-TRANSLATED:8f9ca364]
     */
    void onRtpSorted(RtpPacket::Ptr rtp, int track_idx) override;

    /**
     * 解析出rtp但还未排序
     * @param rtp rtp数据包
     * @param track_index track索引
     * Parse out rtp but not yet sorted
     * @param rtp rtp data packet
     * @param track_index track index
     
     * [AUTO-TRANSLATED:c1636911]
     */
    void onBeforeRtpSorted(const RtpPacket::Ptr &rtp, int track_index) override;

    /**
     * 收到RTCP包回调
     * @param track_idx track索引
     * @param track sdp相关信息
     * @param data rtcp内容
     * @param len rtcp内容长度
     * Callback for receiving RTCP packet
     * @param track_idx track index
     * @param track sdp related information
     * @param data rtcp content
     * @param len rtcp content length
     
     * [AUTO-TRANSLATED:1a2cfa4f]
     */
    virtual void onRtcpPacket(int track_idx, SdpTrack::Ptr &track, uint8_t *data, size_t len);

    /////////////TcpClient override/////////////
    void onConnect(const toolkit::SockException &err) override;
    void onRecv(const toolkit::Buffer::Ptr &buf) override;
    void onError(const toolkit::SockException &ex) override;

private:
    void onPlayResult_l(const toolkit::SockException &ex , bool handshake_done);

    int getTrackIndexByInterleaved(int interleaved) const;
    int getTrackIndexByTrackType(TrackType track_type) const;

    void handleResSETUP(const Parser &parser, unsigned int track_idx);
    void handleResDESCRIBE(const Parser &parser);
    bool handleAuthenticationFailure(const std::string &wwwAuthenticateParamsStr);
    void handleResPAUSE(const Parser &parser, int type);
    bool handleResponse(const std::string &cmd, const Parser &parser);

    void sendOptions();
    void sendSetup(unsigned int track_idx);
    void sendPause(int type , uint32_t ms);
    void sendDescribe();
    void sendTeardown();
    void sendKeepAlive();
    void sendRtspRequest(const std::string &cmd, const std::string &url ,const StrCaseMap &header = StrCaseMap());
    void sendRtspRequest(const std::string &cmd, const std::string &url ,const std::initializer_list<std::string> &header);
    void createUdpSockIfNecessary(int track_idx);

private:
    // 是否为性能测试模式  [AUTO-TRANSLATED:1fde8234]
    // Whether it is performance test mode
    bool _benchmark_mode = false;
    // 轮流发送rtcp与GET_PARAMETER保活  [AUTO-TRANSLATED:5b6f9c37]
    // Send rtcp and GET_PARAMETER keep-alive in turn
    bool _send_rtcp[2] = {true, true};

    // 心跳类型  [AUTO-TRANSLATED:c22abb05]
    // Heartbeat type
    uint32_t _beat_type = 0;
    // 心跳保护间隔  [AUTO-TRANSLATED:de16d9c9]
    // Heartbeat protection interval
    uint32_t _beat_interval_ms = 0;

    std::string _play_url;
    // rtsp开始倍速  [AUTO-TRANSLATED:9ab84508]
    // Rtsp start speed
    float _speed= 0.0f;
    std::vector<SdpTrack::Ptr> _sdp_track;
    std::function<void(const Parser&)> _on_response;
    // RTP端口,trackid idx 为数组下标  [AUTO-TRANSLATED:77c186bb]
    // RTP port, trackid idx is the array subscript
    toolkit::Socket::Ptr _rtp_sock[2];
    // RTCP端口,trackid idx 为数组下标  [AUTO-TRANSLATED:446a7861]
    // RTCP port, trackid idx is the array subscript
    toolkit::Socket::Ptr _rtcp_sock[2];

    // rtsp鉴权相关  [AUTO-TRANSLATED:947dc6a3]
    // Rtsp authentication related
    std::string _md5_nonce;
    std::string _realm;
    //rtsp info
    std::string _session_id;
    uint32_t _cseq_send = 1;
    std::string _content_base;
    std::string _control_url;
    Rtsp::eRtpType _rtp_type = Rtsp::RTP_TCP;

    // 当前rtp时间戳  [AUTO-TRANSLATED:410f2691]
    // Current rtp timestamp
    uint32_t _stamp[2] = {0, 0};

    // 超时功能实现  [AUTO-TRANSLATED:1d603b3a]
    // Timeout function implementation
    toolkit::Ticker _rtp_recv_ticker;
    std::shared_ptr<toolkit::Timer> _play_check_timer;
    std::shared_ptr<toolkit::Timer> _rtp_check_timer;
    // 服务器支持的命令  [AUTO-TRANSLATED:f7f589bf]
    // Server supported commands
    std::set<std::string> _supported_cmd;
    ////////// rtcp ////////////////
    // rtcp发送时间,trackid idx 为数组下标  [AUTO-TRANSLATED:bf3248b1]
    // Rtcp send time, trackid idx is the array subscript
    toolkit::Ticker _rtcp_send_ticker[2];
    // 统计rtp并发送rtcp  [AUTO-TRANSLATED:0ac2b665]
    // Statistics rtp and send rtcp
    std::vector<RtcpContext::Ptr> _rtcp_context;
};

} /* namespace mediakit */
#endif /* SRC_RTSPPLAYER_RTSPPLAYER_H_TXT_ */
