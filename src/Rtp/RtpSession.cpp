/*
 * Copyright (c) 2016 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/xiongziliang/ZLMediaKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#if defined(ENABLE_RTPPROXY)
#include "RtpSession.h"
#include "RtpSelector.h"
namespace mediakit{

RtpSession::RtpSession(const Socket::Ptr &sock) : TcpSession(sock) {
    DebugP(this);
    socklen_t addr_len = sizeof(addr);
    getpeername(sock->rawFD(), &addr, &addr_len);
}
RtpSession::~RtpSession() {
    DebugP(this);
    if(_process){
        RtpSelector::Instance().delProcess(_ssrc,_process.get());
    }
}

void RtpSession::onRecv(const Buffer::Ptr &data) {
    try {
        RtpSplitter::input(data->data(), data->size());
    } catch (SockException &ex) {
        shutdown(ex);
    } catch (std::exception &ex) {
        shutdown(SockException(Err_other, ex.what()));
    }
}

void RtpSession::onError(const SockException &err) {
    WarnL << _ssrc << " " << err.what();
}

void RtpSession::onManager() {
    if(_process && !_process->alive()){
        shutdown(SockException(Err_timeout, "receive rtp timeout"));
    }

    if(!_process && _ticker.createdTime() > 10 * 1000){
        shutdown(SockException(Err_timeout, "illegal connection"));
    }
}

void RtpSession::onRtpPacket(const char *data, uint64_t len) {
    if (!_process) {
        if (!RtpSelector::getSSRC(data + 2, len - 2, _ssrc)) {
            return;
        }
        _process = RtpSelector::Instance().getProcess(_ssrc, true);
        _process->setListener(dynamic_pointer_cast<RtpSession>(shared_from_this()));
    }
    _process->inputRtp(_sock,data + 2, len - 2, &addr);
    _ticker.resetTime();
}

bool RtpSession::close(MediaSource &sender, bool force) {
    //此回调在其他线程触发
    if(!_process || (!force && _process->totalReaderCount())){
        return false;
    }
    string err = StrPrinter << "close media:" << sender.getSchema() << "/" << sender.getVhost() << "/" << sender.getApp() << "/" << sender.getId() << " " << force;
    safeShutdown(SockException(Err_shutdown,err));
    return true;
}

int RtpSession::totalReaderCount(MediaSource &sender) {
    //此回调在其他线程触发
    return _process ? _process->totalReaderCount() : sender.totalReaderCount();
}

}//namespace mediakit
#endif//defined(ENABLE_RTPPROXY)