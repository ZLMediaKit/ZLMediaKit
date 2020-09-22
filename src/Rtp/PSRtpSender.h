/*
 * Copyright (c) 2016 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/xiongziliang/ZLMediaKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#ifndef ZLMEDIAKIT_PSRTPSENDER_H
#define ZLMEDIAKIT_PSRTPSENDER_H
#if defined(ENABLE_RTPPROXY)
#include "PSEncoder.h"
#include "Extension/CommonRtp.h"

namespace mediakit{

class RingDelegateHelper : public RingDelegate<RtpPacket::Ptr> {
public:
    typedef function<void(const RtpPacket::Ptr &in, bool is_key)> onRtp;

    ~RingDelegateHelper() override{}
    RingDelegateHelper(onRtp on_rtp){
        _on_rtp = std::move(on_rtp);
    }
    void onWrite(const RtpPacket::Ptr &in, bool is_key) override{
        _on_rtp(in, is_key);
    }

private:
    onRtp _on_rtp;
};

//该类在PSEncoder的基础上，实现了mpeg-ps的rtp打包以及发送
class PSRtpSender : public PSEncoder, public std::enable_shared_from_this<PSRtpSender>, public PacketCache<RtpPacket>{
public:
    typedef std::shared_ptr<PSRtpSender> Ptr;

    /**
     * 构造函数
     * @param ssrc rtp的ssrc
     * @param payload_type 国标中ps-rtp的pt一般为96
     */
    PSRtpSender(uint32_t ssrc, uint8_t payload_type = 96);
    ~PSRtpSender() override;

    /**
     * 开始发送ps-rtp包
     * @param dst_url 目标ip或域名
     * @param dst_port 目标端口
     * @param is_udp 是否采用udp方式发送rtp
     * @param cb 连接目标端口是否成功的回调
     */
    void startSend(const string &dst_url, uint16_t dst_port, bool is_udp, const function<void(const SockException &ex)> &cb);

    /**
     * 输入帧数据
     */
    void inputFrame(const Frame::Ptr &frame) override;

protected:
    //mpeg-ps回调
    void onPS(uint32_t stamp, void *packet, size_t bytes) override;

    /**
     * 批量flush rtp包时触发该函数
     * @param rtp_list rtp包列表
     * @param key_pos 是否包含关键帧
     */
    void onFlush(std::shared_ptr<List<RtpPacket::Ptr> > &rtp_list, bool key_pos) override;

private:
    //rtp打包后回调
    void onRtp(const RtpPacket::Ptr &in, bool is_key);
    //udp/tcp连接成功回调
    void onConnect();
    //异常断开socket事件
    void onErr(const SockException &ex, bool is_connect = false);

private:
    bool _is_udp;
    bool _is_connect = false;
    string _dst_url;
    uint16_t _dst_port;
    Socket::Ptr _socket;
    EventPoller::Ptr _poller;
    Timer::Ptr _connect_timer;
    std::shared_ptr<CommonRtpEncoder> _rtp_encoder;
};

}//namespace mediakit
#endif// defined(ENABLE_RTPPROXY)
#endif //ZLMEDIAKIT_PSRTPSENDER_H
