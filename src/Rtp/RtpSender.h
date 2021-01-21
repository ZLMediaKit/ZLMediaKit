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

namespace mediakit{

//rtp发送客户端，支持发送GB28181协议
class RtpSender : public MediaSinkInterface, public std::enable_shared_from_this<RtpSender>{
public:
    typedef std::shared_ptr<RtpSender> Ptr;

    ~RtpSender() override;

    /**
     * 构造函数，创建GB28181 RTP发送客户端
     * @param ssrc rtp的ssrc
     * @param payload_type 国标中ps-rtp的pt一般为96
     */
    RtpSender(uint32_t ssrc, uint8_t payload_type = 96);

    /**
     * 开始发送ps-rtp包
     * @param dst_url 目标ip或域名
     * @param dst_port 目标端口
     * @param is_udp 是否采用udp方式发送rtp
     * @param cb 连接目标端口是否成功的回调
     */
    void startSend(const string &dst_url, uint16_t dst_port, bool is_udp, uint16_t src_port, const function<void(uint16_t local_port, const SockException &ex)> &cb);

    /**
     * 输入帧数据
     */
    void inputFrame(const Frame::Ptr &frame) override;

    /**
     * 添加track，内部会调用Track的clone方法
     * 只会克隆sps pps这些信息 ，而不会克隆Delegate相关关系
     * @param track
     */
    virtual void addTrack(const Track::Ptr & track) override;

    /**
     * 添加所有Track完毕
     */
    virtual void addTrackCompleted() override;

    /**
     * 重置track
     */
    virtual void resetTracks() override;

private:
    //合并写输出
    void onFlushRtpList(std::shared_ptr<List<Buffer::Ptr> > rtp_list);
    //udp/tcp连接成功回调
    void onConnect();
    //异常断开socket事件
    void onErr(const SockException &ex, bool is_connect = false);

private:
    bool _is_udp;
    bool _is_connect = false;
    string _dst_url;
    uint16_t _dst_port;
	uint16_t _src_port;
    Socket::Ptr _socket;
    EventPoller::Ptr _poller;
    Timer::Ptr _connect_timer;
    MediaSinkInterface::Ptr _interface;
};

}//namespace mediakit
#endif// defined(ENABLE_RTPPROXY)
#endif //ZLMEDIAKIT_RTPSENDER_H
