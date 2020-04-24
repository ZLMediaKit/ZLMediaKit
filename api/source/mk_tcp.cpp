/*
 * Copyright (c) 2016 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/xiongziliang/ZLMediaKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#include "string.h"
#include "mk_tcp.h"
#include "mk_tcp_private.h"
#include "Http/WebSocketClient.h"
#include "Http/WebSocketSession.h"
using namespace mediakit;

API_EXPORT const char* API_CALL mk_sock_info_peer_ip(const mk_sock_info ctx, char *buf){
    assert(ctx);
    SockInfo *sock = (SockInfo *)ctx;
    strcpy(buf,sock->get_peer_ip().c_str());
    return buf;
}
API_EXPORT const char* API_CALL mk_sock_info_local_ip(const mk_sock_info ctx, char *buf){
    assert(ctx);
    SockInfo *sock = (SockInfo *)ctx;
    strcpy(buf,sock->get_peer_ip().c_str());
    return buf;
}
API_EXPORT uint16_t API_CALL mk_sock_info_peer_port(const mk_sock_info ctx){
    assert(ctx);
    SockInfo *sock = (SockInfo *)ctx;
    return sock->get_peer_port();
}
API_EXPORT uint16_t API_CALL mk_sock_info_local_port(const mk_sock_info ctx){
    assert(ctx);
    SockInfo *sock = (SockInfo *)ctx;
    return sock->get_local_port();
}

////////////////////////////////////////////////////////////////////////////////////////
API_EXPORT mk_sock_info API_CALL mk_tcp_session_get_sock_info(const mk_tcp_session ctx){
    assert(ctx);
    TcpSessionForC *session = (TcpSessionForC *)ctx;
    return (SockInfo *)session;
}

API_EXPORT void API_CALL mk_tcp_session_shutdown(const mk_tcp_session ctx,int err,const char *err_msg){
    assert(ctx);
    TcpSessionForC *session = (TcpSessionForC *)ctx;
    session->safeShutdown(SockException((ErrCode)err,err_msg));
}

API_EXPORT void API_CALL mk_tcp_session_send(const mk_tcp_session ctx,const char *data,int len){
    assert(ctx && data);
    if(!len){
        len = strlen(data);
    }
    TcpSessionForC *session = (TcpSessionForC *)ctx;
    session->SockSender::send(data,len);
}

API_EXPORT void API_CALL mk_tcp_session_send_safe(const mk_tcp_session ctx,const char *data,int len){
    assert(ctx && data);
    if(!len){
        len = strlen(data);
    }
    try {
        weak_ptr<TcpSession> weak_session = ((TcpSessionForC *)ctx)->shared_from_this();
        string str = string(data,len);
        ((TcpSessionForC *)ctx)->async([weak_session,str](){
            auto session_session = weak_session.lock();
            if(session_session){
                session_session->SockSender::send(str);
            }
        });
    }catch (std::exception &ex){
        WarnL << "can not got the strong pionter of this mk_tcp_session:" << ex.what();
    }
}

////////////////////////////////////////TcpSessionForC////////////////////////////////////////////////
static TcpServer::Ptr s_tcp_server[4];
static mk_tcp_session_events s_events_server = {0};

TcpSessionForC::TcpSessionForC(const Socket::Ptr &pSock) : TcpSession(pSock) {
    _local_port = get_local_port();
    if (s_events_server.on_mk_tcp_session_create) {
        s_events_server.on_mk_tcp_session_create(_local_port,this);
    }
}

void TcpSessionForC::onRecv(const Buffer::Ptr &buffer) {
    if (s_events_server.on_mk_tcp_session_data) {
        s_events_server.on_mk_tcp_session_data(_local_port,this, buffer->data(), buffer->size());
    }
}

void TcpSessionForC::onError(const SockException &err) {
    if (s_events_server.on_mk_tcp_session_disconnect) {
        s_events_server.on_mk_tcp_session_disconnect(_local_port,this, err.getErrCode(), err.what());
    }
}

void TcpSessionForC::onManager() {
    if (s_events_server.on_mk_tcp_session_manager) {
        s_events_server.on_mk_tcp_session_manager(_local_port,this);
    }
}

void stopAllTcpServer(){
    CLEAR_ARR(s_tcp_server);
}

API_EXPORT void API_CALL mk_tcp_session_set_user_data(mk_tcp_session session,void *user_data){
    assert(session);
    TcpSessionForC *obj = (TcpSessionForC *)session;
    obj->_user_data = user_data;
}

API_EXPORT void* API_CALL mk_tcp_session_get_user_data(mk_tcp_session session){
    assert(session);
    TcpSessionForC *obj = (TcpSessionForC *)session;
    return obj->_user_data;
}

API_EXPORT void API_CALL mk_tcp_server_events_listen(const mk_tcp_session_events *events){
    if (events) {
        memcpy(&s_events_server, events, sizeof(s_events_server));
    } else {
        memset(&s_events_server, 0, sizeof(s_events_server));
    }
}

API_EXPORT uint16_t API_CALL mk_tcp_server_start(uint16_t port, mk_tcp_type type){
    type = MAX(mk_type_tcp, MIN(type, mk_type_wss));
    try {
        s_tcp_server[type] = std::make_shared<TcpServer>();
        switch (type) {
            case mk_type_tcp:
                s_tcp_server[type]->start<TcpSessionForC>(port);
                break;
            case mk_type_ssl:
                s_tcp_server[type]->start<TcpSessionWithSSL<TcpSessionForC> >(port);
                break;
            case mk_type_ws:
                s_tcp_server[type]->start<WebSocketSession<TcpSessionForC, HttpSession>>(port);
                break;
            case mk_type_wss:
                s_tcp_server[type]->start<WebSocketSession<TcpSessionForC, HttpsSession>>(port);
                break;
            default:
                return 0;
        }
        return s_tcp_server[type]->getPort();
    } catch (std::exception &ex) {
        s_tcp_server[type].reset();
        WarnL << ex.what();
        return 0;
    }
}

///////////////////////////////////////////////////TcpClientForC/////////////////////////////////////////////////////////
TcpClientForC::TcpClientForC(mk_tcp_client_events *events){
    _events = *events;
}


void TcpClientForC::onRecv(const Buffer::Ptr &pBuf) {
    if(_events.on_mk_tcp_client_data){
        _events.on_mk_tcp_client_data(_client,pBuf->data(),pBuf->size());
    }
}

void TcpClientForC::onErr(const SockException &ex) {
    if(_events.on_mk_tcp_client_disconnect){
        _events.on_mk_tcp_client_disconnect(_client,ex.getErrCode(),ex.what());
    }
}

void TcpClientForC::onManager() {
    if(_events.on_mk_tcp_client_manager){
        _events.on_mk_tcp_client_manager(_client);
    }
}

void TcpClientForC::onConnect(const SockException &ex) {
    if(_events.on_mk_tcp_client_connect){
        _events.on_mk_tcp_client_connect(_client,ex.getErrCode(),ex.what());
    }
}

TcpClientForC::~TcpClientForC() {
    TraceL << "mk_tcp_client_release:" << _client;
}

void TcpClientForC::setClient(mk_tcp_client client) {
    _client = client;
    TraceL << "mk_tcp_client_create:" << _client;
}

TcpClientForC::Ptr *mk_tcp_client_create_l(mk_tcp_client_events *events, mk_tcp_type type){
    assert(events);
    type = MAX(mk_type_tcp, MIN(type, mk_type_wss));
    switch (type) {
        case mk_type_tcp:
            return new TcpClientForC::Ptr(new TcpClientForC(events));
        case mk_type_ssl:
            return (TcpClientForC::Ptr *)new shared_ptr<TcpSessionWithSSL<TcpClientForC> >(new TcpSessionWithSSL<TcpClientForC>(events));
        case mk_type_ws:
            return (TcpClientForC::Ptr *)new shared_ptr<WebSocketClient<TcpClientForC, WebSocketHeader::TEXT, false> >(new WebSocketClient<TcpClientForC, WebSocketHeader::TEXT, false>(events));
        case mk_type_wss:
            return (TcpClientForC::Ptr *)new shared_ptr<WebSocketClient<TcpClientForC, WebSocketHeader::TEXT, true> >(new WebSocketClient<TcpClientForC, WebSocketHeader::TEXT, true>(events));
        default:
            return nullptr;
    }
}

API_EXPORT mk_sock_info API_CALL mk_tcp_client_get_sock_info(const mk_tcp_client ctx){
    assert(ctx);
    TcpClientForC::Ptr *client = (TcpClientForC::Ptr *)ctx;
    return (SockInfo *)client->get();
}

API_EXPORT mk_tcp_client API_CALL mk_tcp_client_create(mk_tcp_client_events *events, mk_tcp_type type){
    auto ret = mk_tcp_client_create_l(events,type);
    (*ret)->setClient(ret);
    return ret;
}

API_EXPORT void API_CALL mk_tcp_client_release(mk_tcp_client ctx){
    assert(ctx);
    TcpClientForC::Ptr *client = (TcpClientForC::Ptr *)ctx;
    delete client;
}

API_EXPORT void API_CALL mk_tcp_client_connect(mk_tcp_client ctx, const char *host, uint16_t port, float time_out_sec){
    assert(ctx);
    TcpClientForC::Ptr *client = (TcpClientForC::Ptr *)ctx;
    (*client)->startConnect(host,port);
}

API_EXPORT void API_CALL mk_tcp_client_send(mk_tcp_client ctx, const char *data, int len){
    assert(ctx && data);
    TcpClientForC::Ptr *client = (TcpClientForC::Ptr *)ctx;
    (*client)->SockSender::send(data,len);
}

API_EXPORT void API_CALL mk_tcp_client_send_safe(mk_tcp_client ctx, const char *data, int len){
    assert(ctx && data);
    TcpClientForC::Ptr *client = (TcpClientForC::Ptr *)ctx;
    weak_ptr<TcpClient> weakClient = *client;
    Buffer::Ptr buf = (*client)->obtainBuffer(data,len);
    (*client)->async([weakClient,buf](){
        auto strongClient = weakClient.lock();
        if(strongClient){
            strongClient->send(buf);
        }
    });
}

API_EXPORT void API_CALL mk_tcp_client_set_user_data(mk_tcp_client ctx,void *user_data){
    assert(ctx);
    TcpClientForC::Ptr *client = (TcpClientForC::Ptr *)ctx;
    (*client)->_user_data = user_data;
}

API_EXPORT void* API_CALL mk_tcp_client_get_user_data(mk_tcp_client ctx){
    assert(ctx);
    TcpClientForC::Ptr *client = (TcpClientForC::Ptr *)ctx;
    return (*client)->_user_data;
}
