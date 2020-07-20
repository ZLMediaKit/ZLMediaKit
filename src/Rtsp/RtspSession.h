/*
 * Copyright (c) 2016 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/xiongziliang/ZLMediaKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#ifndef SESSION_RTSPSESSION_H_
#define SESSION_RTSPSESSION_H_

#include <set>
#include <vector>
#include <unordered_set>
#include <unordered_map>
#include "Util/util.h"
#include "Util/logger.h"
#include "Common/config.h"
#include "Network/TcpSession.h"
#include "Player/PlayerBase.h"
#include "RtpMultiCaster.h"
#include "RtspMediaSource.h"
#include "RtspSplitter.h"
#include "RtpReceiver.h"
#include "RtspMediaSourceImp.h"
#include "Common/Stamp.h"

using namespace std;
using namespace toolkit;

namespace mediakit {

class RtspSession;

class BufferRtp : public Buffer{
public:
    typedef std::shared_ptr<BufferRtp> Ptr;
    BufferRtp(const RtpPacket::Ptr & pkt,uint32_t offset = 0 ):_rtp(pkt),_offset(offset){}
    virtual ~BufferRtp(){}

    char *data() const override {
        return (char *)_rtp->data() + _offset;
    }
    uint32_t size() const override {
        return _rtp->size() - _offset;
    }
private:
    RtpPacket::Ptr _rtp;
    uint32_t _offset;
};

class RtspSession: public TcpSession, public RtspSplitter, public RtpReceiver , public MediaSourceEvent{
public:
    typedef std::shared_ptr<RtspSession> Ptr;
    typedef std::function<void(const string &realm)> onGetRealm;
    //encrypted为true是则表明是md5加密的密码，否则是明文密码
    //在请求明文密码时如果提供md5密码者则会导致认证失败
    typedef std::function<void(bool encrypted,const string &pwd_or_md5)> onAuth;

    RtspSession(const Socket::Ptr &sock);
    virtual ~RtspSession();
    ////TcpSession override////
    void onRecv(const Buffer::Ptr &buf) override;
    void onError(const SockException &err) override;
    void onManager() override;

protected:
    /////RtspSplitter override/////
    //收到完整的rtsp包回调，包括sdp等content数据
    void onWholeRtspPacket(Parser &parser) override;
    //收到rtp包回调
    void onRtpPacket(const char *data, uint64_t len) override;
    //从rtsp头中获取Content长度
    int64_t getContentLength(Parser &parser) override;
    ////RtpReceiver override////
    void onRtpSorted(const RtpPacket::Ptr &rtp, int track_idx) override;
    ////MediaSourceEvent override////
    bool close(MediaSource &sender, bool force) override ;
    int totalReaderCount(MediaSource &sender) override;
    /////TcpSession override////
    int send(const Buffer::Ptr &pkt) override;
    //收到RTCP包回调
    virtual void onRtcpPacket(int track_idx, SdpTrack::Ptr &track, unsigned char *data, unsigned int len);

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
    int getTrackIndexByControlSuffix(const string &control_suffix);
    int getTrackIndexByInterleaved(int interleaved);
    //一般用于接收udp打洞包，也用于rtsp推流
    void onRcvPeerUdpData(int interleaved, const Buffer::Ptr &buf, const struct sockaddr &addr);
    //配合onRcvPeerUdpData使用
    void startListenPeerUdpData(int track_idx);
    ////rtsp专有认证相关////
    //认证成功
    void onAuthSuccess();
    //认证失败
    void onAuthFailed(const string &realm, const string &why, bool close = true);
    //开始走rtsp专有认证流程
    void onAuthUser(const string &realm, const string &authorization);
    //校验base64方式的认证加密
    void onAuthBasic(const string &realm, const string &auth_base64);
    //校验md5方式的认证加密
    void onAuthDigest(const string &realm, const string &auth_md5);
    //触发url鉴权事件
    void emitOnPlay();
    //发送rtp给客户端
    void sendRtpPacket(const RtspMediaSource::RingDataType &pkt);
    //触发rtcp发送
    void onSendRtpPacket(const RtpPacket::Ptr &rtp);
    //回复客户端
    bool sendRtspResponse(const string &res_code, const std::initializer_list<string> &header, const string &sdp = "", const char *protocol = "RTSP/1.0");
    bool sendRtspResponse(const string &res_code, const StrCaseMap &header = StrCaseMap(), const string &sdp = "", const char *protocol = "RTSP/1.0");
    //服务器发送rtcp
    void sendSenderReport(bool over_tcp, int track_idx);
    //设置socket标志
    void setSocketFlags();

private:
    //是否已经触发on_play事件
    bool _emit_on_play = false;
    //是否开始发送rtp
    bool _enable_send_rtp;
    //推流或拉流客户端采用的rtp传输方式
    Rtsp::eRtpType _rtp_type = Rtsp::RTP_Invalid;
    //收到的seq，回复时一致
    int _cseq = 0;
    //rtsp推流起始时间戳，目的是为了同步
    int64_t _start_stamp[2] = {-1, -1};
    //消耗的总流量
    uint64_t _bytes_usage = 0;
    //ContentBase
    string _content_base;
    //Session号
    string _sessionid;
    //记录是否需要rtsp专属鉴权，防止重复触发事件
    string _rtsp_realm;
    //登录认证
    string _auth_nonce;
    //用于判断客户端是否超时
    Ticker _alive_ticker;

    //url解析后保存的相关信息
    MediaInfo _media_info;
    //rtsp推流相关绑定的源
    RtspMediaSourceImp::Ptr _push_src;
    //rtsp播放器绑定的直播源
    std::weak_ptr<RtspMediaSource> _play_src;
    //直播源读取器
    RtspMediaSource::RingType::RingReader::Ptr _play_reader;
    //sdp里面有效的track,包含音频或视频
    vector<SdpTrack::Ptr> _sdp_track;

    //rtcp统计,trackid idx 为数组下标
    RtcpCounter _rtcp_counter[2];
    //rtcp发送时间,trackid idx 为数组下标
    Ticker _rtcp_send_tickers[2];

    ////////RTP over udp////////
    //RTP端口,trackid idx 为数组下标
    Socket::Ptr _rtp_socks[2];
    //RTCP端口,trackid idx 为数组下标
    Socket::Ptr _rtcp_socks[2];
    //标记是否收到播放的udp打洞包,收到播放的udp打洞包后才能知道其外网udp端口号
    unordered_set<int> _udp_connected_flags;
    ////////RTP over udp_multicast////////
    //共享的rtp组播对象
    RtpMultiCaster::Ptr _multicaster;
    ////////RTSP over HTTP  ////////
    //quicktime 请求rtsp会产生两次tcp连接，
    //一次发送 get 一次发送post，需要通过x-sessioncookie关联起来
    string _http_x_sessioncookie;
    function<void(const Buffer::Ptr &)> _on_recv;
};

/**
 * 支持ssl加密的rtsp服务器，可用于诸如亚马逊echo show这样的设备访问
 */
typedef TcpSessionWithSSL<RtspSession> RtspSessionWithSSL;

} /* namespace mediakit */

#endif /* SESSION_RTSPSESSION_H_ */
