/*
 * Copyright (c) 2016 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/xia-chu/ZLMediaKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#ifndef SRC_RTSPPLAYER_RTSPPLAYER_H_TXT_
#define SRC_RTSPPLAYER_RTSPPLAYER_H_TXT_

#include <string>
#include <memory>
#include "RtspSession.h"
#include "RtspMediaSource.h"
#include "Player/PlayerBase.h"
#include "Util/util.h"
#include "Util/logger.h"
#include "Util/TimeTicker.h"
#include "Poller/Timer.h"
#include "Network/Socket.h"
#include "Network/TcpClient.h"
#include "RtspSplitter.h"
#include "RtpReceiver.h"
#include "Common/Stamp.h"
#include "Rtcp/RtcpContext.h"

namespace mediakit {

//实现了rtsp播放器协议部分的功能，及数据接收功能
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
    //派生类回调函数
    virtual bool onCheckSDP(const std::string &sdp) = 0;
    virtual void onRecvRTP(RtpPacket::Ptr rtp, const SdpTrack::Ptr &track) = 0;
    uint32_t getProgressMilliSecond() const;
    void seekToMilliSecond(uint32_t ms);

    /**
     * 收到完整的rtsp包回调，包括sdp等content数据
     * @param parser rtsp包
     */
    void onWholeRtspPacket(Parser &parser) override ;

    /**
     * 收到rtp包回调
     * @param data
     * @param len
     */
    void onRtpPacket(const char *data,size_t len) override ;

    /**
     * rtp数据包排序后输出
     * @param rtp rtp数据包
     * @param track_idx track索引
     */
    void onRtpSorted(RtpPacket::Ptr rtp, int track_idx) override;

    /**
     * 解析出rtp但还未排序
     * @param rtp rtp数据包
     * @param track_index track索引
     */
    void onBeforeRtpSorted(const RtpPacket::Ptr &rtp, int track_index) override;

    /**
     * 收到RTCP包回调
     * @param track_idx track索引
     * @param track sdp相关信息
     * @param data rtcp内容
     * @param len rtcp内容长度
     */
    virtual void onRtcpPacket(int track_idx, SdpTrack::Ptr &track, uint8_t *data, size_t len);

    /////////////TcpClient override/////////////
    void onConnect(const toolkit::SockException &err) override;
    void onRecv(const toolkit::Buffer::Ptr &buf) override;
    void onErr(const toolkit::SockException &ex) override;

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
    //是否为性能测试模式
    bool _benchmark_mode = false;
    //轮流发送rtcp与GET_PARAMETER保活
    bool _send_rtcp[2] = {true, true};

    std::string _play_url;
    std::vector<SdpTrack::Ptr> _sdp_track;
    std::function<void(const Parser&)> _on_response;
    //RTP端口,trackid idx 为数组下标
    toolkit::Socket::Ptr _rtp_sock[2];
    //RTCP端口,trackid idx 为数组下标
    toolkit::Socket::Ptr _rtcp_sock[2];

    //rtsp鉴权相关
    std::string _md5_nonce;
    std::string _realm;
    //rtsp info
    std::string _session_id;
    uint32_t _cseq_send = 1;
    std::string _content_base;
    Rtsp::eRtpType _rtp_type = Rtsp::RTP_TCP;

    //当前rtp时间戳
    uint32_t _stamp[2] = {0, 0};

    //超时功能实现
    toolkit::Ticker _rtp_recv_ticker;
    std::shared_ptr<toolkit::Timer> _play_check_timer;
    std::shared_ptr<toolkit::Timer> _rtp_check_timer;
    //服务器支持的命令
    std::set<std::string> _supported_cmd;
    ////////// rtcp ////////////////
    //rtcp发送时间,trackid idx 为数组下标
    toolkit::Ticker _rtcp_send_ticker[2];
    //统计rtp并发送rtcp
    std::vector<RtcpContext::Ptr> _rtcp_context;
};

} /* namespace mediakit */
#endif /* SRC_RTSPPLAYER_RTSPPLAYER_H_TXT_ */
