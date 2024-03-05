/*
 * Copyright (c) 2016-present The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/ZLMediaKit/ZLMediaKit).
 *
 * Use of this source code is governed by MIT-like license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#if defined(ENABLE_RTPPROXY)
#include "RtpSession.h"
#include "RtpSelector.h"
#include "Network/TcpServer.h"
#include "Rtsp/Rtsp.h"
#include "Rtsp/RtpReceiver.h"
#include "Common/config.h"

using namespace std;
using namespace toolkit;

namespace mediakit{

const string RtpSession::kStreamID = "stream_id";
const string RtpSession::kSSRC = "ssrc";
const string RtpSession::kOnlyTrack = "only_track";
const string RtpSession::kUdpRecvBuffer = "udp_recv_socket_buffer";

void RtpSession::attachServer(const Server &server) {
    setParams(const_cast<Server &>(server));
}

void RtpSession::setParams(mINI &ini) {
    _stream_id = ini[kStreamID];
    _ssrc = ini[kSSRC];
    _only_track = ini[kOnlyTrack];
    int udp_socket_buffer = ini[kUdpRecvBuffer];
    if (_is_udp) {
        // 设置udp socket读缓存
        SockUtil::setRecvBuf(getSock()->rawFD(),
            (udp_socket_buffer > 0) ? udp_socket_buffer : (4 * 1024 * 1024));
    }
}

RtpSession::RtpSession(const Socket::Ptr &sock)
    : Session(sock) {
    socklen_t addr_len = sizeof(_addr);
    getpeername(sock->rawFD(), (struct sockaddr *)&_addr, &addr_len);
    _is_udp = sock->sockType() == SockNum::Sock_UDP;
}

RtpSession::~RtpSession() = default;

void RtpSession::onRecv(const Buffer::Ptr &data) {
    if (_is_udp) {
        onRtpPacket(data->data(), data->size());
        return;
    }
    RtpSplitter::input(data->data(), data->size());
}

void RtpSession::onError(const SockException &err) {
    WarnP(this) << _stream_id << " " << err;
    if (_process) {
        RtpSelector::Instance().delProcess(_stream_id, _process.get());
        _process = nullptr;
    }
}

void RtpSession::onManager() {
    if (_process && !_process->alive()) {
        shutdown(SockException(Err_timeout, "receive rtp timeout"));
    }

    if (!_process && _ticker.createdTime() > 10 * 1000) {
        shutdown(SockException(Err_timeout, "illegal connection"));
    }
}

void RtpSession::onRtpPacket(const char *data, size_t len) {
    if (_delay_close) {
        // 正在延时关闭中，忽略所有数据
        return;
    }
    if (!isRtp(data, len)) {
        // 忽略非rtp数据
        WarnP(this) << "Not rtp packet";
        return;
    }
    if (!_is_udp) {
        if (_search_rtp) {
            //搜索上下文期间，数据丢弃
            if (_search_rtp_finished) {
                //下个包开始就是正确的rtp包了
                _search_rtp_finished = false;
                _search_rtp = false;
            }
            return;
        }
        GET_CONFIG(uint32_t, rtpMaxSize, Rtp::kRtpMaxSize);
        if (len > 1024 * rtpMaxSize) {
            _search_rtp = true;
            WarnL << "rtp包长度异常(" << len << ")，发送端可能缓存溢出并覆盖，开始搜索ssrc以便恢复上下文";
            return;
        }
    }
    if (!_process) {
        //未设置ssrc时，尝试获取ssrc
        if (!_ssrc && !RtpSelector::getSSRC(data, len, _ssrc)) {
            return;
        }
        if (_stream_id.empty()) {
            //未指定流id就使用ssrc为流id
            _stream_id = printSSRC(_ssrc);
        }
        try {
            _process = RtpSelector::Instance().getProcess(_stream_id, true);
        } catch (RtpSelector::ProcessExisted &ex) {
            if (!_is_udp) {
                // tcp情况下立即断开连接
                throw;
            }
            // udp情况下延时断开连接(等待超时自动关闭)，防止频繁创建销毁RtpSession对象
            WarnP(this) << ex.what();
            _delay_close = true;
            return;
        }
        _process->setOnlyTrack((RtpProcess::OnlyTrack)_only_track);
        _process->setDelegate(static_pointer_cast<RtpSession>(shared_from_this()));
    }
    try {
        uint32_t rtp_ssrc = 0;
        RtpSelector::getSSRC(data, len, rtp_ssrc);
        if (rtp_ssrc != _ssrc) {
            WarnP(this) << "ssrc mismatched, rtp dropped: " << rtp_ssrc << " != " << _ssrc;
            return;
        }
        _process->inputRtp(false, getSock(), data, len, (struct sockaddr *)&_addr);
    } catch (RtpTrack::BadRtpException &ex) {
        if (!_is_udp) {
            WarnL << ex.what() << "，开始搜索ssrc以便恢复上下文";
            _search_rtp = true;
        } else {
            throw;
        }
    } catch (std::exception &ex) {
        if (!_is_udp) {
            // tcp情况下立即断开连接
            throw;
        }
        // udp情况下延时断开连接(等待超时自动关闭)，防止频繁创建销毁RtpSession对象
        WarnP(this) << ex.what();
        _delay_close = true;
        return;
    }
    _ticker.resetTime();
}

bool RtpSession::close(MediaSource &sender) {
    //此回调在其他线程触发
    string err = StrPrinter << "close media: " << sender.getUrl();
    safeShutdown(SockException(Err_shutdown, err));
    return true;
}

static const char *findSSRC(const char *data, ssize_t len, uint32_t ssrc) {
    // rtp前面必须预留两个字节的长度字段
    for (ssize_t i = 2; i <= len - 4; ++i) {
        auto ptr = (const uint8_t *)data + i;
        if (ptr[0] == (ssrc >> 24) && ptr[1] == ((ssrc >> 16) & 0xFF) && ptr[2] == ((ssrc >> 8) & 0xFF)
            && ptr[3] == (ssrc & 0xFF)) {
            return (const char *)ptr;
        }
    }
    return nullptr;
}

static const char *findPsHeaderFlag(const char *data, ssize_t len) {
    for (ssize_t i = 2; i <= len - 4; ++i) {
        auto ptr = (const uint8_t *)data + i;
        // PsHeader 0x000001ba、PsSystemHeader0x000001bb（关键帧标识）
        if (ptr[0] == (0x00) && ptr[1] == (0x00) && ptr[2] == (0x01) && ptr[3] == (0xbb)) {
            return (const char *)ptr;
        }
    }

    return nullptr;
}

// rtp长度到ssrc间的长度固定为10
static size_t constexpr kSSRCOffset = 2 + 4 + 4;
// rtp长度到ps header间的长度固定为14 （暂时不采用找ps header,采用找system header代替）
// rtp长度到ps system header间的长度固定为20 (关键帧标识)
static size_t constexpr kPSHeaderOffset = 2 + 4 + 4 + 4 + 20;

const char *RtpSession::onSearchPacketTail(const char *data, size_t len) {
    if (!_search_rtp) {
        // tcp上下文正常，不用搜索ssrc
        return RtpSplitter::onSearchPacketTail(data, len);
    }
    if (!_process) {
        InfoL << "ssrc未获取到，无法通过ssrc恢复tcp上下文；尝试搜索PsSystemHeader恢复tcp上下文。";
        auto rtp_ptr1 = searchByPsHeaderFlag(data, len);
        return rtp_ptr1;
    }
    auto rtp_ptr0 = searchBySSRC(data, len);
    if (rtp_ptr0) {
        return rtp_ptr0;
    }
    // ssrc搜索失败继续尝试搜索ps header flag
    auto rtp_ptr2 = searchByPsHeaderFlag(data, len);
    return rtp_ptr2;
}

const char *RtpSession::searchBySSRC(const char *data, size_t len) {
    InfoL << "尝试rtp搜索ssrc..._ssrc=" << _ssrc;
    // 搜索第一个rtp的ssrc
    auto ssrc_ptr0 = findSSRC(data, len, _ssrc);
    if (!ssrc_ptr0) {
        // 未搜索到任意rtp，返回数据不够
        InfoL << "rtp搜索ssrc失败（第一个数据不够），丢弃rtp数据为：" << len;
        return nullptr;
    }
    // 这两个字节是第一个rtp的长度字段
    auto rtp_len_ptr = (ssrc_ptr0 - kSSRCOffset);
    auto rtp_len = ((uint8_t *)rtp_len_ptr)[0] << 8 | ((uint8_t *)rtp_len_ptr)[1];

    // 搜索第二个rtp的ssrc
    auto ssrc_ptr1 = findSSRC(ssrc_ptr0 + rtp_len, data + (ssize_t)len - ssrc_ptr0 - rtp_len, _ssrc);
    if (!ssrc_ptr1) {
        // 未搜索到第二个rtp，返回数据不够
        InfoL << "rtp搜索ssrc失败(第二个数据不够)，丢弃rtp数据为：" << len;
        return nullptr;
    }

    // 两个ssrc的间隔正好等于rtp的长度(外加rtp长度字段)，那么说明找到rtp
    auto ssrc_offset = ssrc_ptr1 - ssrc_ptr0;
    if (ssrc_offset == rtp_len + 2 || ssrc_offset == rtp_len + 4) {
        InfoL << "rtp搜索ssrc成功，tcp上下文恢复成功，丢弃的rtp残余数据为：" << rtp_len_ptr - data;
        _search_rtp_finished = true;
        if (rtp_len_ptr == data) {
            // 停止搜索rtp，否则会进入死循环
            _search_rtp = false;
        }
        // 前面的数据都需要丢弃，这个是rtp的起始
        return rtp_len_ptr;
    }
    // 第一个rtp长度不匹配，说明第一个找到的ssrc不是rtp，丢弃之，我们从第二个ssrc所在rtp开始搜索
    return ssrc_ptr1 - kSSRCOffset;
}

const char *RtpSession::searchByPsHeaderFlag(const char *data, size_t len) {
    InfoL << "尝试rtp搜索PsSystemHeaderFlag..._ssrc=" << _ssrc;
    // 搜索rtp中的第一个PsHeaderFlag
    auto ps_header_flag_ptr = findPsHeaderFlag(data, len);
    if (!ps_header_flag_ptr) {
        InfoL << "rtp搜索flag失败，丢弃rtp数据为：" << len;
        return nullptr;
    }

    auto rtp_ptr = ps_header_flag_ptr - kPSHeaderOffset;
    _search_rtp_finished = true;
    if (rtp_ptr == data) {
        // 停止搜索rtp，否则会进入死循环
        _search_rtp = false;
    }
    InfoL << "rtp搜索flag成功，tcp上下文恢复成功，丢弃的rtp残余数据为：" << rtp_ptr - data;

    // TODO or Not ? 更新设置ssrc
    uint32_t rtp_ssrc = 0;
    RtpSelector::getSSRC(rtp_ptr + 2, len, rtp_ssrc);
    _ssrc = rtp_ssrc;
    InfoL << "设置_ssrc为：" << _ssrc;
    // RtpServer::updateSSRC(uint32_t ssrc)
    return rtp_ptr;
}

}//namespace mediakit
#endif//defined(ENABLE_RTPPROXY)
