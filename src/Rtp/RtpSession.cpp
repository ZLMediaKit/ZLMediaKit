/*
 * Copyright (c) 2016 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/xia-chu/ZLMediaKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#if defined(ENABLE_RTPPROXY)
#include "RtpSession.h"
#include "RtpSelector.h"
#include "Network/TcpServer.h"
#include "Rtsp/RtpReceiver.h"

using namespace std;
using namespace toolkit;

namespace mediakit{

const string RtpSession::kStreamID = "stream_id";
const string RtpSession::kIsUDP = "is_udp";
const string RtpSession::kSSRC = "ssrc";

void RtpSession::attachServer(const Server &server) {
    _stream_id = const_cast<Server &>(server)[kStreamID];
    _is_udp = const_cast<Server &>(server)[kIsUDP];
    _ssrc = const_cast<Server &>(server)[kSSRC];

    if (_is_udp) {
        //设置udp socket读缓存
        SockUtil::setRecvBuf(getSock()->rawFD(), 4 * 1024 * 1024);
        _statistic_udp = std::make_shared<ObjectStatistic<UdpSession> >();
    } else {
        _statistic_tcp = std::make_shared<ObjectStatistic<TcpSession> >();
    }
}

RtpSession::RtpSession(const Socket::Ptr &sock) : Session(sock) {
    DebugP(this);
    socklen_t addr_len = sizeof(_addr);
    getpeername(sock->rawFD(), (struct sockaddr *)&_addr, &addr_len);
}

RtpSession::~RtpSession() {
    DebugP(this);
    if(_process){
        RtpSelector::Instance().delProcess(_stream_id,_process.get());
    }
}

void RtpSession::onRecv(const Buffer::Ptr &data) {
    if (_is_udp) {
        onRtpPacket(data->data(), data->size());
        return;
    }
    RtpSplitter::input(data->data(), data->size());
}

void RtpSession::onError(const SockException &err) {
    WarnP(this) << _stream_id << " " << err.what();
}

void RtpSession::onManager() {
    if(_process && !_process->alive()){
        shutdown(SockException(Err_timeout, "receive rtp timeout"));
    }

    if(!_process && _ticker.createdTime() > 10 * 1000){
        shutdown(SockException(Err_timeout, "illegal connection"));
    }
}

void RtpSession::onRtpPacket(const char *data, size_t len) {
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
        //tcp情况下，一个tcp链接只可能是一路流，不需要通过多个ssrc来区分，所以不需要频繁getProcess
        _process = RtpSelector::Instance().getProcess(_stream_id, true);
        _process->setDelegate(dynamic_pointer_cast<RtpSession>(shared_from_this()));
    }
    try {
        uint32_t rtp_ssrc = 0;
        RtpSelector::getSSRC(data, len, rtp_ssrc);
        if (rtp_ssrc != _ssrc) {
            WarnP(this) << "ssrc不匹配,rtp已丢弃:" << rtp_ssrc << " != " << _ssrc;
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
    } catch (...) {
        throw;
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
    //rtp前面必须预留两个字节的长度字段
    for (ssize_t i = 2; i <= len - 4; ++i) {
        auto ptr = (const uint8_t *) data + i;
        if (ptr[0] == (ssrc >> 24) && ptr[1] == ((ssrc >> 16) & 0xFF) &&
            ptr[2] == ((ssrc >> 8) & 0xFF) && ptr[3] == (ssrc & 0xFF)) {
            return (const char *) ptr;
        }
    }
    return nullptr;
}

//rtp长度到ssrc间的长度固定为10
static size_t constexpr kSSRCOffset = 2 + 4 + 4;

const char *RtpSession::onSearchPacketTail(const char *data, size_t len) {
    if (!_search_rtp) {
        //tcp上下文正常，不用搜索ssrc
        return RtpSplitter::onSearchPacketTail(data, len);
    }
    if (!_process) {
        throw SockException(Err_shutdown, "ssrc未获取到，无法通过ssrc恢复tcp上下文");
    }
    //搜索第一个rtp的ssrc
    auto ssrc_ptr0 = findSSRC(data, len, _ssrc);
    if (!ssrc_ptr0) {
        //未搜索到任意rtp，返回数据不够
        return nullptr;
    }
    //这两个字节是第一个rtp的长度字段
    auto rtp_len_ptr = (ssrc_ptr0 - kSSRCOffset);
    auto rtp_len = ((uint8_t *)rtp_len_ptr)[0] << 8 | ((uint8_t *)rtp_len_ptr)[1];

    //搜索第二个rtp的ssrc
    auto ssrc_ptr1 = findSSRC(ssrc_ptr0 + rtp_len, data + (ssize_t) len - ssrc_ptr0 - rtp_len, _ssrc);
    if (!ssrc_ptr1) {
        //未搜索到第二个rtp，返回数据不够
        return nullptr;
    }

    //两个ssrc的间隔正好等于rtp的长度(外加rtp长度字段)，那么说明找到rtp
    auto ssrc_offset = ssrc_ptr1 - ssrc_ptr0;
    if (ssrc_offset == rtp_len + 2 || ssrc_offset == rtp_len + 4) {
        InfoL << "rtp搜索成功，tcp上下文恢复成功，丢弃的rtp残余数据为：" << rtp_len_ptr - data;
        _search_rtp_finished = true;
        if (rtp_len_ptr == data) {
            //停止搜索rtp，否则会进入死循环
            _search_rtp = false;
        }
        //前面的数据都需要丢弃，这个是rtp的起始
        return rtp_len_ptr;
    }
    //第一个rtp长度不匹配，说明第一个找到的ssrc不是rtp，丢弃之，我们从第二个ssrc所在rtp开始搜索
    return ssrc_ptr1 - kSSRCOffset;
}

}//namespace mediakit
#endif//defined(ENABLE_RTPPROXY)