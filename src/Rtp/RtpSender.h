/*
 * Copyright (c) 2016-present The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/ZLMediaKit/ZLMediaKit).
 *
 * Use of this source code is governed by MIT-like license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#ifndef ZLMEDIAKIT_RTPSENDER_H
#define ZLMEDIAKIT_RTPSENDER_H
#if defined(ENABLE_RTPPROXY)
#include "PSEncoder.h"
#include "Extension/CommonRtp.h"
#include "Rtcp/RtcpContext.h"
#include "Common/MediaSource.h"
#include "Common/MediaSink.h"

namespace mediakit{

class RtpSession;

// rtp发送客户端，支持发送GB28181协议  [AUTO-TRANSLATED:668038b6]
// RTP sending client, supporting sending GB28181 protocol
class RtpSender final : public MediaSinkInterface, public std::enable_shared_from_this<RtpSender>{
public:
    using Ptr = std::shared_ptr<RtpSender>;

    RtpSender(toolkit::EventPoller::Ptr poller = nullptr);
    ~RtpSender() override;

    /**
     * 开始发送ps-rtp包
     * @param args 发送参数
     * @param cb 连接目标端口是否成功的回调
     * Start sending ps-rtp packets
     * @param args Sending parameters
     * @param cb Callback for whether the connection to the target port is successful
     
     * [AUTO-TRANSLATED:c31bd9b3]
     */
    void startSend(const MediaSourceEvent::SendRtpArgs &args, const std::function<void(uint16_t local_port, const toolkit::SockException &ex)> &cb);

    /**
     * 输入帧数据
     * Input frame data
     
     * [AUTO-TRANSLATED:d13bc7f2]
     */
    bool inputFrame(const Frame::Ptr &frame) override;

    /**
     * 刷新输出frame缓存
     * Refresh the output frame cache
     
     * [AUTO-TRANSLATED:547f851c]
     */
    void flush() override;

    /**
     * 添加track，内部会调用Track的clone方法
     * 只会克隆sps pps这些信息 ，而不会克隆Delegate相关关系
     * @param track
     * Add track, internally calls the clone method of Track
     * Only clones sps pps information, not Delegate relationships
     * @param track
     
     * [AUTO-TRANSLATED:ba6faf58]
     */
    virtual bool addTrack(const Track::Ptr & track) override;

    /**
     * 添加所有Track完毕
     * All Tracks added
     
     * [AUTO-TRANSLATED:751c45ca]
     */
    virtual void addTrackCompleted() override;

    /**
     * 重置track
     * Reset track
     
     * [AUTO-TRANSLATED:95dc0b4f]
     */
    virtual void resetTracks() override;

    /**
     * 设置发送rtp停止回调
     * Set RTP sending stop callback
     
     * [AUTO-TRANSLATED:7e0a6714]
     */
    void setOnClose(std::function<void(const toolkit::SockException &ex)> on_close);

private:
    // 合并写输出  [AUTO-TRANSLATED:23544836]
    // Merge write output
    void onFlushRtpList(std::shared_ptr<toolkit::List<toolkit::Buffer::Ptr> > rtp_list);
    // udp/tcp连接成功回调  [AUTO-TRANSLATED:ca35017d]
    // UDP/TCP connection success callback
    void onConnect();
    // 异常断开socket事件  [AUTO-TRANSLATED:a59cd9de]
    // Abnormal socket disconnect event
    void onErr(const toolkit::SockException &ex);
    void createRtcpSocket();
    void onRecvRtcp(RtcpHeader *rtcp);
    void onSendRtpUdp(const toolkit::Buffer::Ptr &buf, bool check);
    void onClose(const toolkit::SockException &ex);

private:
    bool _is_connect = false;
    MediaSourceEvent::SendRtpArgs _args;
    toolkit::Socket::Ptr _socket_rtp;
    toolkit::Socket::Ptr _socket_rtcp;
    toolkit::EventPoller::Ptr _poller;
    MediaSinkInterface::Ptr _interface;
    std::shared_ptr<RtcpContext> _rtcp_context;
    toolkit::Ticker _rtcp_send_ticker;
    toolkit::Ticker _rtcp_recv_ticker;
    std::shared_ptr<RtpSession> _rtp_session;
    std::function<void(const toolkit::SockException &ex)> _on_close;
};

}//namespace mediakit
#endif// defined(ENABLE_RTPPROXY)
#endif //ZLMEDIAKIT_RTPSENDER_H
