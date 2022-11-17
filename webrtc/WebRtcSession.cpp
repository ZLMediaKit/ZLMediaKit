/*
 * Copyright (c) 2016 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/xia-chu/ZLMediaKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#include "WebRtcSession.h"
#include "Util/util.h"

using namespace std;

namespace mediakit {

static string getUserName(const Buffer::Ptr &buffer) {
    auto buf = buffer->data() + 2;
    auto len = buffer->size() - 2;
    if (!RTC::StunPacket::IsStun((const uint8_t *) buf, len)) {
        return "";
    }
    std::unique_ptr<RTC::StunPacket> packet(RTC::StunPacket::Parse((const uint8_t *) buf, len));
    if (!packet) {
        return "";
    }
    if (packet->GetClass() != RTC::StunPacket::Class::REQUEST ||
        packet->GetMethod() != RTC::StunPacket::Method::BINDING) {
        return "";
    }
    //收到binding request请求
    auto vec = split(packet->GetUsername(), ":");
    return vec[0];
}

EventPoller::Ptr WebRtcSession::queryPoller(const Buffer::Ptr &buffer) {
    auto user_name = getUserName(buffer);
    if (user_name.empty()) {
        return nullptr;
    }
    auto ret = WebRtcTransportManager::Instance().getItem(user_name);
    return ret ? ret->getPoller() : nullptr;
}

////////////////////////////////////////////////////////////////////////////////

WebRtcSession::WebRtcSession(const Socket::Ptr &sock) : TcpSession(sock) {
    socklen_t addr_len = sizeof(_peer_addr);
    getpeername(sock->rawFD(), (struct sockaddr *)&_peer_addr, &addr_len);
}

WebRtcSession::~WebRtcSession() {
    InfoP(this);
}

/*
 * Framing RFC 4571
 *     0                   1                   2                   3
 *     0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
 *     ---------------------------------------------------------------
 *     |             LENGTH            |  RTP or RTCP packet ...   |
 *     ---------------------------------------------------------------
 *      The bit field definition of the framing method
 * A 16-bit unsigned integer LENGTH field, coded in network byte order
 * (big-endian), begins the frame.  If LENGTH is non-zero, an RTP or
 * RTCP packet follows the LENGTH field.  The value coded in the LENGTH
 * field MUST equal the number of octets in the RTP or RTCP packet.
 * Zero is a valid value for LENGTH, and it codes the null packet.
 */

void WebRtcSession::onRecv(const Buffer::Ptr &buffer) {
    //只允许寻找一次transport
    if (!_transport) {
        auto user_name = getUserName(buffer);
        auto transport = WebRtcTransportManager::Instance().getItem(user_name);
        //TODO fix this poller is not current thread
        //CHECK(transport && transport->getPoller()->isCurrentThread());
        transport->setSession(shared_from_this());
        _transport = std::move(transport);
        InfoP(this);
    }
    _ticker.resetTime();
    CHECK(_transport);
    //_transport->inputSockData(buffer->data() + 2, buffer->size() - 2, (struct sockaddr *)&_peer_addr);

    //一个tcp数据包里面可能会有多帧
    uint8_t* buf = reinterpret_cast<uint8_t *>(buffer->data());
    size_t buf_size = buffer->size();
    size_t frame_start = 0;
    size_t remian_len  = 0;
    size_t frame_size = 0;
    for (;;) {
        remian_len = buf_size - frame_start;
        if(remian_len >= 2){
            frame_size = size_t { Utils::Byte::Get2Bytes(buf + frame_start, 0) };
        }
        //解析出来了一帧tcp frame
        if (remian_len >= 2 && remian_len >= 2 + frame_size) {
            const uint8_t *frame = buf + frame_start + 2;
            if(frame_size != 0){
                _transport->inputSockData((char *)frame, frame_size, (struct sockaddr *)&_peer_addr);
            }
            //数据全部解析完毕
            if((frame_start + 2 + frame_size) == buf_size){
                break;
            }
            //更新解析buf的起始位置
            else{
                frame_start += (2 + frame_size);
            }
            //还有数据 需要继续解析
            if (buf_size > frame_start) {
                continue;
            }
            break;
        }
        //包解析出错了 丢弃
        else{
            WarnL<<"Incomplete packet";
            break;
        }
    }
}

void WebRtcSession::onError(const SockException &err) {
    //udp链接超时，但是rtc链接不一定超时，因为可能存在udp链接迁移的情况
    //在udp链接迁移时，新的WebRtcSession对象将接管WebRtcTransport对象的生命周期
    //本WebRtcSession对象将在超时后自动销毁
    WarnP(this) << err.what();

    if (!_transport) {
        return;
    }
    auto transport = std::move(_transport);
    getPoller()->async([transport] {
        //延时减引用，防止使用transport对象时，销毁对象
    }, false);
}

void WebRtcSession::onManager() {
    GET_CONFIG(float, timeoutSec, Rtc::kTimeOutSec);
    if (!_transport && _ticker.createdTime() > timeoutSec * 1000) {
        shutdown(SockException(Err_timeout, "illegal webrtc connection"));
        return;
    }
    if (_ticker.elapsedTime() > timeoutSec * 1000) {
        shutdown(SockException(Err_timeout, "webrtc connection timeout"));
        return;
    }
}

}// namespace mediakit


