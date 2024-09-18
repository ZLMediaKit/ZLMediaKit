/*
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
    // encrypted为true是则表明是md5加密的密码，否则是明文密码  [AUTO-TRANSLATED:cad96e51]
    // `encrypted` being `true` indicates an MD5 encrypted password, otherwise it is a plain text password
    // 在请求明文密码时如果提供md5密码者则会导致认证失败  [AUTO-TRANSLATED:8a38bff8]
    // When requesting a plain text password, providing an MD5 password will result in authentication failure
    using onAuth = std::function<void(bool encrypted, const std::string &pwd_or_md5)>;

    RtspSession(const toolkit::Socket::Ptr &sock);
    ////Session override////
    void onRecv(const toolkit::Buffer::Ptr &buf) override;
    void onError(const toolkit::SockException &err) override;
    void onManager() override;

protected:
    /////RtspSplitter override/////
    // 收到完整的rtsp包回调，包括sdp等content数据  [AUTO-TRANSLATED:efbe20df]
    // Callback for receiving a complete RTSP packet, including SDP and other content data
    void onWholeRtspPacket(Parser &parser) override;
    // 收到rtp包回调  [AUTO-TRANSLATED:119f1cca]
    // Callback for receiving an RTP packet
    void onRtpPacket(const char *data, size_t len) override;
    // 从rtsp头中获取Content长度  [AUTO-TRANSLATED:0e6f033e]
    // Get the Content length from the RTSP header
    ssize_t getContentLength(Parser &parser) override;

    ////RtpReceiver override////
    void onRtpSorted(RtpPacket::Ptr rtp, int track_idx) override;
    void onBeforeRtpSorted(const RtpPacket::Ptr &rtp, int track_index) override;

    ///////MediaSourceEvent override///////
    // 关闭  [AUTO-TRANSLATED:92392f02]
    // Close
    bool close(MediaSource &sender) override;
    // 播放总人数  [AUTO-TRANSLATED:c42a3161]
    // Total number of players
    int totalReaderCount(MediaSource &sender) override;
    // 获取媒体源类型  [AUTO-TRANSLATED:34290a69]
    // Get the media source type
    MediaOriginType getOriginType(MediaSource &sender) const override;
    // 获取媒体源url或者文件路径  [AUTO-TRANSLATED:fa34d795]
    // Get the media source URL or file path
    std::string getOriginUrl(MediaSource &sender) const override;
    // 获取媒体源客户端相关信息  [AUTO-TRANSLATED:037ef910]
    // Get the media source client related information
    std::shared_ptr<SockInfo> getOriginSock(MediaSource &sender) const override;
    // 由于支持断连续推，存在OwnerPoller变更的可能  [AUTO-TRANSLATED:1c863b40]
    // Due to support for continuous pushing, there is a possibility of OwnerPoller changes
    toolkit::EventPoller::Ptr getOwnerPoller(MediaSource &sender) override;

    /////Session override////
    ssize_t send(toolkit::Buffer::Ptr pkt) override;
    // 收到RTCP包回调  [AUTO-TRANSLATED:249f4807]
    // Callback for receiving an RTCP packet
    virtual void onRtcpPacket(int track_idx, SdpTrack::Ptr &track, const char *data, size_t len);

    // 回复客户端  [AUTO-TRANSLATED:8108ebea]
    // Reply to the client
    virtual bool sendRtspResponse(const std::string &res_code, const StrCaseMap &header = StrCaseMap(), const std::string &sdp = "", const char *protocol = "RTSP/1.0");

protected:
    // url解析后保存的相关信息  [AUTO-TRANSLATED:1c26e4e3]
    // Information related to the URL after parsing
    MediaInfo _media_info;

    ////////RTP over udp_multicast////////
    // 共享的rtp组播对象  [AUTO-TRANSLATED:d4a5cfdd]
    // Shared RTP multicast object
    RtpMultiCaster::Ptr _multicaster;

    // Session号  [AUTO-TRANSLATED:4552ec74]
    // Session number
    std::string _sessionid;

    uint32_t _multicast_ip = 0;
    uint16_t _multicast_video_port = 0;
    uint16_t _multicast_audio_port = 0;

private:
    // 处理options方法,获取服务器能力  [AUTO-TRANSLATED:a51f6d7c]
    // Handle the OPTIONS method, get server capabilities
    void handleReq_Options(const Parser &parser);
    // 处理describe方法，请求服务器rtsp sdp信息  [AUTO-TRANSLATED:ed2c8fcb]
    // Handle the DESCRIBE method, request server RTSP SDP information
    void handleReq_Describe(const Parser &parser);
    // 处理ANNOUNCE方法，请求推流，附带sdp  [AUTO-TRANSLATED:aa4b4517]
    // Handle the ANNOUNCE method, request streaming, with SDP attached
    void handleReq_ANNOUNCE(const Parser &parser);
    // 处理record方法，开始推流  [AUTO-TRANSLATED:885cf8a9]
    // Handle the RECORD method, start streaming
    void handleReq_RECORD(const Parser &parser);
    // 处理setup方法，播放和推流协商rtp传输方式用  [AUTO-TRANSLATED:cbe5dcfc]
    // Handle the SETUP method, used for negotiating RTP transport methods for playback and streaming
    void handleReq_Setup(const Parser &parser);
    // 处理play方法，开始或恢复播放  [AUTO-TRANSLATED:f15d151d]
    // Handle the PLAY method, start or resume playback
    void handleReq_Play(const Parser &parser);
    // 处理pause方法，暂停播放  [AUTO-TRANSLATED:0c3b8f79]
    // Handle the PAUSE method, pause playback
    void handleReq_Pause(const Parser &parser);
    // 处理teardown方法，结束播放  [AUTO-TRANSLATED:64d82572]
    // Handle the TEARDOWN method, end playback
    void handleReq_Teardown(const Parser &parser);
    // 处理Get方法,rtp over http才用到  [AUTO-TRANSLATED:c7c51eb6]
    // Handle the GET method, only used for RTP over HTTP
    void handleReq_Get(const Parser &parser);
    // 处理Post方法，rtp over http才用到  [AUTO-TRANSLATED:228bdbbe]
    // Handle the POST method, only used for RTP over HTTP
    void handleReq_Post(const Parser &parser);
    // 处理SET_PARAMETER、GET_PARAMETER方法，一般用于心跳  [AUTO-TRANSLATED:b9e333e1]
    // Handle the SET_PARAMETER, GET_PARAMETER methods, generally used for heartbeats
    void handleReq_SET_PARAMETER(const Parser &parser);
    // rtsp资源未找到  [AUTO-TRANSLATED:9b779890]
    // RTSP resource not found
    void send_StreamNotFound();
    // 不支持的传输模式  [AUTO-TRANSLATED:ef90414c]
    // Unsupported transport mode
    void send_UnsupportedTransport();
    // 会话id错误  [AUTO-TRANSLATED:7cf632d3]
    // Session ID error
    void send_SessionNotFound();
    // 一般rtsp服务器打开端口失败时触发  [AUTO-TRANSLATED:82ecb043]
    // Triggered when the general RTSP server fails to open the port
    void send_NotAcceptable();
    // 获取track下标  [AUTO-TRANSLATED:36d0b2c2]
    // Get the track index
    int getTrackIndexByTrackType(TrackType type);
    int getTrackIndexByControlUrl(const std::string &control_url);
    int getTrackIndexByInterleaved(int interleaved);
    // 一般用于接收udp打洞包，也用于rtsp推流  [AUTO-TRANSLATED:0b55c12f]
    // Generally used to receive UDP hole punching packets, also used for RTSP pushing
    void onRcvPeerUdpData(int interleaved, const toolkit::Buffer::Ptr &buf, const struct sockaddr_storage &addr);
    // 配合onRcvPeerUdpData使用  [AUTO-TRANSLATED:811d2d1a]
    // Used in conjunction with onRcvPeerUdpData
    void startListenPeerUdpData(int track_idx);
    // //rtsp专有认证相关////  [AUTO-TRANSLATED:0f021bb5]
    // // RTSP specific authentication related ////
    // 认证成功  [AUTO-TRANSLATED:e1bafff3]
    // Authentication successful
    void onAuthSuccess();
    // 认证失败  [AUTO-TRANSLATED:a188326a]
    // Authentication failed
    void onAuthFailed(const std::string &realm, const std::string &why, bool close = true);
    // 开始走rtsp专有认证流程  [AUTO-TRANSLATED:2d773497]
    // Start the RTSP specific authentication process
    void onAuthUser(const std::string &realm, const std::string &authorization);
    // 校验base64方式的认证加密  [AUTO-TRANSLATED:bde8662f]
    // Verify base64 authentication encryption
    void onAuthBasic(const std::string &realm, const std::string &auth_base64);
    // 校验md5方式的认证加密  [AUTO-TRANSLATED:0cc37fa7]
    // Verify MD5 authentication encryption
    void onAuthDigest(const std::string &realm, const std::string &auth_md5);
    // 触发url鉴权事件  [AUTO-TRANSLATED:776dc4b5]
    // Trigger URL authentication event
    void emitOnPlay();
    // 发送rtp给客户端  [AUTO-TRANSLATED:18602be0]
    // Send RTP to the client
    void sendRtpPacket(const RtspMediaSource::RingDataType &pkt);
    // 触发rtcp发送  [AUTO-TRANSLATED:4fbe7706]
    // Trigger RTCP sending
    void updateRtcpContext(const RtpPacket::Ptr &rtp);
    // 回复客户端  [AUTO-TRANSLATED:8108ebea]
    // Reply to the client
    bool sendRtspResponse(const std::string &res_code, const std::initializer_list<std::string> &header, const std::string &sdp = "", const char *protocol = "RTSP/1.0");

    // 设置socket标志  [AUTO-TRANSLATED:4086e686]
    // Set socket flag
    void setSocketFlags();

private:
    // 是否已经触发on_play事件  [AUTO-TRANSLATED:49c937ce]
    // Whether the on_play event has been triggered
    bool _emit_on_play = false;
    bool _send_sr_rtcp[2] = {true, true};
    // 断连续推延时  [AUTO-TRANSLATED:13ad578a]
    // Delay in continuous pushing
    uint32_t _continue_push_ms = 0;
    // 推流或拉流客户端采用的rtp传输方式  [AUTO-TRANSLATED:27411079]
    // RTP transport method used by the pushing or pulling client
    Rtsp::eRtpType _rtp_type = Rtsp::RTP_Invalid;
    // 收到的seq，回复时一致  [AUTO-TRANSLATED:64544fb4]
    // Received seq, consistent when replying
    int _cseq = 0;
    // 消耗的总流量  [AUTO-TRANSLATED:45ad2785]
    // Total traffic consumed
    uint64_t _bytes_usage = 0;
    //ContentBase
    std::string _content_base;
    // 记录是否需要rtsp专属鉴权，防止重复触发事件  [AUTO-TRANSLATED:9cff90b9]
    // Record whether RTSP specific authentication is required to prevent duplicate event triggering
    std::string _rtsp_realm;
    // 登录认证  [AUTO-TRANSLATED:43fdb875]
    // Login authentication
    std::string _auth_nonce;
    // 用于判断客户端是否超时  [AUTO-TRANSLATED:86cb328a]
    // Used to determine if the client has timed out
    toolkit::Ticker _alive_ticker;

    // rtsp推流相关绑定的源  [AUTO-TRANSLATED:a25078d9]
    // Source bound to RTSP pushing
    RtspMediaSourceImp::Ptr _push_src;
    // 推流器所有权  [AUTO-TRANSLATED:e47b4bcb]
    // Pusher ownership
    std::shared_ptr<void> _push_src_ownership;
    // rtsp播放器绑定的直播源  [AUTO-TRANSLATED:a7e130b5]
    // Live source bound to the RTSP player
    std::weak_ptr<RtspMediaSource> _play_src;
    // 直播源读取器  [AUTO-TRANSLATED:e1edc193]
    // Live source reader
    RtspMediaSource::RingType::RingReader::Ptr _play_reader;
    // sdp里面有效的track,包含音频或视频  [AUTO-TRANSLATED:64e2fcdf]
    // Valid track in SDP, including audio or video
    std::vector<SdpTrack::Ptr> _sdp_track;
    // 播放器setup指定的播放track,默认为TrackInvalid表示不指定即音视频都推  [AUTO-TRANSLATED:c7a0df5e]
    // Track specified by the player setup, default is TrackInvalid, which means no specification, both audio and video are pushed
    TrackType _target_play_track = TrackInvalid;

    ////////RTP over udp////////
    // RTP端口,trackid idx 为数组下标  [AUTO-TRANSLATED:77c186bb]
    // RTP port, trackid idx is the array index
    toolkit::Socket::Ptr _rtp_socks[2];
    // RTCP端口,trackid idx 为数组下标  [AUTO-TRANSLATED:446a7861]
    // RTCP port, trackid idx is the array index
    toolkit::Socket::Ptr _rtcp_socks[2];
    // 标记是否收到播放的udp打洞包,收到播放的udp打洞包后才能知道其外网udp端口号  [AUTO-TRANSLATED:ad039c25]
    // Flag whether the UDP hole punching packet for playback has been received. The external UDP port number can only be known after receiving the UDP hole punching packet for playback.
    std::unordered_set<int> _udp_connected_flags;
    ////////RTSP over HTTP  ////////
    // quicktime 请求rtsp会产生两次tcp连接，  [AUTO-TRANSLATED:3f72e181]
    // QuickTime requests for RTSP will generate two TCP connections,
    // 一次发送 get 一次发送post，需要通过x-sessioncookie关联起来  [AUTO-TRANSLATED:f29a653f]
    // one for sending GET and one for sending POST. They need to be associated through x-sessioncookie.
    std::string _http_x_sessioncookie;
    std::function<void(const toolkit::Buffer::Ptr &)> _on_recv;
    ////////// rtcp ////////////////
    // rtcp发送时间,trackid idx 为数组下标  [AUTO-TRANSLATED:bf3248b1]
    // RTCP send time, trackid idx is the array index
    toolkit::Ticker _rtcp_send_tickers[2];
    // 统计rtp并发送rtcp  [AUTO-TRANSLATED:0ac2b665]
    // Count RTP and send RTCP
    std::vector<RtcpContext::Ptr> _rtcp_context;
};

/**
 * 支持ssl加密的rtsp服务器，可用于诸如亚马逊echo show这样的设备访问
 * RTSP server supporting SSL encryption, which can be used for devices such as Amazon Echo Show to access.
 
 
 * [AUTO-TRANSLATED:7d1eed83]
 */
using RtspSessionWithSSL = toolkit::SessionWithSSL<RtspSession>;

} /* namespace mediakit */

#endif /* SESSION_RTSPSESSION_H_ */
