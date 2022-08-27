/*
 * Copyright (c) 2016 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/xia-chu/ZLMediaKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#ifndef ZLMEDIAKIT_RTPSENDER_H
#define ZLMEDIAKIT_RTPSENDER_H
#if defined(ENABLE_RTPPROXY)
#include "PSEncoder.h"
#include "Extension/CommonRtp.h"
#include "Rtcp/RtcpContext.h"

namespace mediakit{

//rtp发送客户端，支持发送GB28181协议
class RtpSender : public MediaSinkInterface, public std::enable_shared_from_this<RtpSender>{
public:
    typedef std::shared_ptr<RtpSender> Ptr;

    RtpSender(toolkit::EventPoller::Ptr poller = nullptr);
    ~RtpSender() override = default;

    /**
     * 开始发送ps-rtp包
     * @param args 发送参数
     * @param cb 连接目标端口是否成功的回调
     */
    void startSend(const MediaSourceEvent::SendRtpArgs &args, const std::function<void(uint16_t local_port, const toolkit::SockException &ex)> &cb);

    /**
     * 输入帧数据
     */
    bool inputFrame(const Frame::Ptr &frame) override;

    /**
     * 添加track，内部会调用Track的clone方法
     * 只会克隆sps pps这些信息 ，而不会克隆Delegate相关关系
     * @param track
     */
    virtual bool addTrack(const Track::Ptr & track) override;

    /**
     * 添加所有Track完毕
     */
    virtual void addTrackCompleted() override;

    /**
     * 重置track
     */
    virtual void resetTracks() override;

    void setOnClose(std::function<void(const toolkit::SockException &ex)> on_close);

private:
    //合并写输出
    void onFlushRtpList(std::shared_ptr<toolkit::List<toolkit::Buffer::Ptr> > rtp_list);
    //udp/tcp连接成功回调
    void onConnect();
    //异常断开socket事件
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
    std::function<void(const toolkit::SockException &ex)> _on_close;
};

}//namespace mediakit
#endif// defined(ENABLE_RTPPROXY)
#endif //ZLMEDIAKIT_RTPSENDER_H
