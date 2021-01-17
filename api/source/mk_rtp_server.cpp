/*
 * Copyright (c) 2016 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/xia-chu/ZLMediaKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#include "mk_rtp_server.h"
#include "Util/logger.h"
using namespace toolkit;

#if defined(ENABLE_RTPPROXY)
#include "Rtp/RtpServer.h"
using namespace mediakit;

API_EXPORT mk_rtp_server API_CALL mk_rtp_server_create(uint16_t port, int enable_tcp, const char *stream_id){
    RtpServer::Ptr *server = new RtpServer::Ptr(new RtpServer);
    (*server)->start(port, stream_id, enable_tcp);
    return server;
}

API_EXPORT void API_CALL mk_rtp_server_release(mk_rtp_server ctx){
    RtpServer::Ptr *server = (RtpServer::Ptr *)ctx;
    delete server;
}

API_EXPORT uint16_t API_CALL mk_rtp_server_port(mk_rtp_server ctx){
    RtpServer::Ptr *server = (RtpServer::Ptr *)ctx;
    return (*server)->getPort();
}

API_EXPORT void API_CALL mk_rtp_server_set_on_detach(mk_rtp_server ctx, on_mk_rtp_server_detach cb, void *user_data){
    RtpServer::Ptr *server = (RtpServer::Ptr *) ctx;
    if (cb) {
        (*server)->setOnDetach([cb, user_data]() {
            cb(user_data);
        });
    } else {
        (*server)->setOnDetach(nullptr);
    }
}

#else

API_EXPORT mk_rtp_server API_CALL mk_rtp_server_create(uint16_t port, int enable_tcp, const char *stream_id){
    WarnL << "请打开ENABLE_RTPPROXY后再编译";
    return nullptr;
}

API_EXPORT void API_CALL mk_rtp_server_release(mk_rtp_server ctx){
    WarnL << "请打开ENABLE_RTPPROXY后再编译";
}

API_EXPORT uint16_t API_CALL mk_rtp_server_port(mk_rtp_server ctx){
    WarnL << "请打开ENABLE_RTPPROXY后再编译";
    return 0;
}

API_EXPORT void API_CALL mk_rtp_server_set_on_detach(mk_rtp_server ctx, on_mk_rtp_server_detach cb, void *user_data){
    WarnL << "请打开ENABLE_RTPPROXY后再编译";
}

#endif