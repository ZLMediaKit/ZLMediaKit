﻿/*
 * Copyright (c) 2016-present The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/ZLMediaKit/ZLMediaKit).
 *
 * Use of this source code is governed by MIT-like license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#ifndef SESSION_RTSPSESSION_H_
#define SESSION_RTSPSESSION_H_

#include <set>
#include <vector>
#include <unordered_set>
#include "Network/Session.h"
#include "RtspSplitter.h"
#include "RtpReceiver.h"
#include "Rtcp/RtcpContext.h"
#include "RtspMediaSource.h"
#include "RtspMediaSourceImp.h"
#include "RtpMultiCaster.h"

namespace mediakit {

using BufferRtp = toolkit::BufferOffset<toolkit::Buffer::Ptr>;
class RtspSession : public toolkit::Session, public RtspSplitter, public RtpReceiver, public MediaSourceEvent {
public:
    using Ptr = std::shared_ptr<RtspSession>;
    using onGetRealm = std::function<void(const std::string &realm)>;
    //encrypted为true是则表明是md5加密的密码，否则是明文密码
    //在请求明文密码时如果提供md5密码者则会导致认证失败
    using onAuth = std::function<void(bool encrypted, const std::string &pwd_or_md5)>;

    RtspSession(const toolkit::Socket::Ptr &sock);
    ////Session override////
    void onRecv(const toolkit::Buffer::Ptr &buf) override;
    void onError(const toolkit::SockException &err) override;
    void onManager() override;

protected:
    /////RtspSplitter override/////
    //收到完整的rtsp包回调，包括sdp等content数据
    void onWholeRtspPacket(Parser &parser) override;
    //收到rtp包回调
    void onRtpPacket(const char *data, size_t len) override;
    //从rtsp头中获取Content长度
    ssize_t getContentLength(Parser &parser) override;

    ////RtpReceiver override////
    void onRtpSorted(RtpPacket::Ptr rtp, int track_idx) override;
    void onBeforeRtpSorted(const RtpPacket::Ptr &rtp, int track_index) override;

    ///////MediaSourceEvent override///////
    // 关闭
    bool close(MediaSource &sender) override;
    // 播放总人数
    int totalReaderCount(MediaSource &sender) override;
    // 获取媒体源类型
    MediaOriginType getOriginType(MediaSource &sender) const override;
    // 获取媒体源url或者文件路径
    std::string getOriginUrl(MediaSource &sender) const override;
    // 获取媒体源客户端相关信息
    std::shared_ptr<SockInfo> getOriginSock(MediaSource &sender) const override;
    // 由于支持断连续推，存在OwnerPoller变更的可能
    toolkit::EventPoller::Ptr getOwnerPoller(MediaSource &sender) override;

    /////Session override////
    ssize_t send(toolkit::Buffer::Ptr pkt) override;
    //收到RTCP包回调
    virtual void onRtcpPacket(int track_idx, SdpTrack::Ptr &track, const char *data, size_t len);

    //回复客户端
    virtual bool sendRtspResponse(const std::string &res_code, const StrCaseMap &header = StrCaseMap(), const std::string &sdp = "", const char *protocol = "RTSP/1.0");

protected:
    //url解析后保存的相关信息
    MediaInfo _media_info;

    ////////RTP over udp_multicast////////
    //共享的rtp组播对象
    RtpMultiCaster::Ptr _multicaster;

    //Session号
    std::string _sessionid;

    uint32_t _multicast_ip = 0;
    uint16_t _multicast_video_port = 0;
    uint16_t _multicast_audio_port = 0;

private:
    //处理options方法,获取服务器能力
    void handleReq_Options(const Parser &parser);
    //处理describe方法，请求服务器rtsp sdp信息
    void handleReq_Describe(const Parser &parser);
    //处理ANNOUNCE方法，请求推流，附带sdp
    void handleReq_ANNOUNCE(const Parser &parser);
    //处理record方法，开始推流
    void handleReq_RECORD(const Parser &parser);
    //处理setup方法，播放和推流协商rtp传输方式用
    void handleReq_Setup(const Parser &parser);
    //处理play方法，开始或恢复播放
    void handleReq_Play(const Parser &parser);
    //处理pause方法，暂停播放
    void handleReq_Pause(const Parser &parser);
    //处理teardown方法，结束播放
    void handleReq_Teardown(const Parser &parser);
    //处理Get方法,rtp over http才用到
    void handleReq_Get(const Parser &parser);
    //处理Post方法，rtp over http才用到
    void handleReq_Post(const Parser &parser);
    //处理SET_PARAMETER、GET_PARAMETER方法，一般用于心跳
    void handleReq_SET_PARAMETER(const Parser &parser);
    //rtsp资源未找到
    void send_StreamNotFound();
    //不支持的传输模式
    void send_UnsupportedTransport();
    //会话id错误
    void send_SessionNotFound();
    //一般rtsp服务器打开端口失败时触发
    void send_NotAcceptable();
    //获取track下标
    int getTrackIndexByTrackType(TrackType type);
    int getTrackIndexByControlUrl(const std::string &control_url);
    int getTrackIndexByInterleaved(int interleaved);
    //一般用于接收udp打洞包，也用于rtsp推流
    void onRcvPeerUdpData(int interleaved, const toolkit::Buffer::Ptr &buf, const struct sockaddr_storage &addr);
    //配合onRcvPeerUdpData使用
    void startListenPeerUdpData(int track_idx);
    ////rtsp专有认证相关////
    //认证成功
    void onAuthSuccess();
    //认证失败
    void onAuthFailed(const std::string &realm, const std::string &why, bool close = true);
    //开始走rtsp专有认证流程
    void onAuthUser(const std::string &realm, const std::string &authorization);
    //校验base64方式的认证加密
    void onAuthBasic(const std::string &realm, const std::string &auth_base64);
    //校验md5方式的认证加密
    void onAuthDigest(const std::string &realm, const std::string &auth_md5);
    //触发url鉴权事件
    void emitOnPlay();
    //发送rtp给客户端
    void sendRtpPacket(const RtspMediaSource::RingDataType &pkt);
    //触发rtcp发送
    void updateRtcpContext(const RtpPacket::Ptr &rtp);
    //回复客户端
    bool sendRtspResponse(const std::string &res_code, const std::initializer_list<std::string> &header, const std::string &sdp = "", const char *protocol = "RTSP/1.0");

    //设置socket标志
    void setSocketFlags();

private:
    //是否已经触发on_play事件
    bool _emit_on_play = false;
    bool _send_sr_rtcp[2] = {true, true};
    //断连续推延时
    uint32_t _continue_push_ms = 0;
    //推流或拉流客户端采用的rtp传输方式
    Rtsp::eRtpType _rtp_type = Rtsp::RTP_Invalid;
    //收到的seq，回复时一致
    int _cseq = 0;
    //消耗的总流量
    uint64_t _bytes_usage = 0;
    //ContentBase
    std::string _content_base;
    //记录是否需要rtsp专属鉴权，防止重复触发事件
    std::string _rtsp_realm;
    //登录认证
    std::string _auth_nonce;
    //用于判断客户端是否超时
    toolkit::Ticker _alive_ticker;

    //rtsp推流相关绑定的源
    RtspMediaSourceImp::Ptr _push_src;
    //推流器所有权
    std::shared_ptr<void> _push_src_ownership;
    //rtsp播放器绑定的直播源
    std::weak_ptr<RtspMediaSource> _play_src;
    //直播源读取器
    RtspMediaSource::RingType::RingReader::Ptr _play_reader;
    //sdp里面有效的track,包含音频或视频
    std::vector<SdpTrack::Ptr> _sdp_track;
    //播放器setup指定的播放track,默认为TrackInvalid表示不指定即音视频都推
    TrackType _target_play_track = TrackInvalid;

    ////////RTP over udp////////
    //RTP端口,trackid idx 为数组下标
    toolkit::Socket::Ptr _rtp_socks[2];
    //RTCP端口,trackid idx 为数组下标
    toolkit::Socket::Ptr _rtcp_socks[2];
    //标记是否收到播放的udp打洞包,收到播放的udp打洞包后才能知道其外网udp端口号
    std::unordered_set<int> _udp_connected_flags;
    ////////RTSP over HTTP  ////////
    //quicktime 请求rtsp会产生两次tcp连接，
    //一次发送 get 一次发送post，需要通过x-sessioncookie关联起来
    std::string _http_x_sessioncookie;
    std::function<void(const toolkit::Buffer::Ptr &)> _on_recv;
    ////////// rtcp ////////////////
    //rtcp发送时间,trackid idx 为数组下标
    toolkit::Ticker _rtcp_send_tickers[2];
    //统计rtp并发送rtcp
    std::vector<RtcpContext::Ptr> _rtcp_context;
};

/**
 * 支持ssl加密的rtsp服务器，可用于诸如亚马逊echo show这样的设备访问
 */
using RtspSessionWithSSL = toolkit::SessionWithSSL<RtspSession>;

} /* namespace mediakit */

#endif /* SESSION_RTSPSESSION_H_ */
