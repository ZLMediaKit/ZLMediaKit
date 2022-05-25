/*
 * Copyright (c) 2016 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/xia-chu/ZLMediaKit).
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
#include "Network/Buffer.h"

using namespace toolkit;
using namespace mediakit;

class BufferForC : public Buffer {
public:
    BufferForC(const char *data, size_t len, on_mk_buffer_free cb, void *user_data) {
        if (len <= 0) {
            len = strlen(data);
        }
        if (!cb) {
            auto ptr = malloc(len);
            memcpy(ptr, data, len);
            data = (char *) ptr;

            cb = [](void *user_data, void *data) {
                free(data);
            };
        }
        _data = (char *) data;
        _size = len;
        _cb = cb;
        _user_data = user_data;
    }

    ~BufferForC() override {
        _cb(_user_data, _data);
    }

    char *data() const override {
        return _data;
    }

    size_t size() const override {
        return _size;
    }

private:
    char *_data;
    size_t _size;
    on_mk_buffer_free _cb;
    void *_user_data;
};

API_EXPORT mk_buffer API_CALL mk_buffer_from_char(const char *data, size_t len, on_mk_buffer_free cb, void *user_data) {
    assert(data);
    return new Buffer::Ptr(std::make_shared<BufferForC>(data, len, cb, user_data));
}

API_EXPORT mk_buffer API_CALL mk_buffer_ref(mk_buffer buffer) {
    assert(buffer);
    return new Buffer::Ptr(*((Buffer::Ptr *) buffer));
}

API_EXPORT void API_CALL mk_buffer_unref(mk_buffer buffer) {
    assert(buffer);
    delete (Buffer::Ptr *) buffer;
}

API_EXPORT const char *API_CALL mk_buffer_get_data(mk_buffer buffer) {
    assert(buffer);
    return (*((Buffer::Ptr *) buffer))->data();
}

API_EXPORT size_t API_CALL mk_buffer_get_size(mk_buffer buffer) {
    assert(buffer);
    return (*((Buffer::Ptr *) buffer))->size();
}

API_EXPORT const char* API_CALL mk_sock_info_peer_ip(const mk_sock_info ctx, char *buf){
    assert(ctx);
    SockInfo *sock = (SockInfo *)ctx;
    strcpy(buf,sock->get_peer_ip().c_str());
    return buf;
}
API_EXPORT const char* API_CALL mk_sock_info_local_ip(const mk_sock_info ctx, char *buf){
    assert(ctx);
    SockInfo *sock = (SockInfo *)ctx;
    strcpy(buf,sock->get_local_ip().c_str());
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

API_EXPORT void API_CALL mk_tcp_session_send_buffer(const mk_tcp_session ctx, mk_buffer buffer) {
    assert(ctx && buffer);
    TcpSessionForC *session = (TcpSessionForC *) ctx;
    session->send(*((Buffer::Ptr *) buffer));
}

API_EXPORT void API_CALL mk_tcp_session_send(const mk_tcp_session ctx, const char *data, size_t len) {
    auto buffer = mk_buffer_from_char(data, len, nullptr, nullptr);
    mk_tcp_session_send_buffer(ctx, buffer);
    mk_buffer_unref(buffer);
}

API_EXPORT void API_CALL mk_tcp_session_send_buffer_safe(const mk_tcp_session ctx, mk_buffer buffer) {
    assert(ctx && buffer);
    try {
        std::weak_ptr<TcpSession> weak_session = ((TcpSessionForC *) ctx)->shared_from_this();
        auto ref = mk_buffer_ref(buffer);
        ((TcpSessionForC *) ctx)->async([weak_session, ref]() {
            auto session_session = weak_session.lock();
            if (session_session) {
                session_session->send(*((Buffer::Ptr *) ref));
            }
            mk_buffer_unref(ref);
        });
    } catch (std::exception &ex) {
        WarnL << "can not got the strong pionter of this mk_tcp_session:" << ex.what();
    }
}

API_EXPORT mk_tcp_session_ref API_CALL mk_tcp_session_ref_from(const mk_tcp_session ctx) {
    auto ref = ((TcpSessionForC *) ctx)->shared_from_this();
    return new std::shared_ptr<TcpSessionForC>(std::dynamic_pointer_cast<TcpSessionForC>(ref));
}

API_EXPORT void mk_tcp_session_ref_release(const mk_tcp_session_ref ref) {
    delete (std::shared_ptr<TcpSessionForC> *) ref;
}

API_EXPORT mk_tcp_session mk_tcp_session_from_ref(const mk_tcp_session_ref ref) {
    return ((std::shared_ptr<TcpSessionForC> *) ref)->get();
}

API_EXPORT void API_CALL mk_tcp_session_send_safe(const mk_tcp_session ctx, const char *data, size_t len) {
    auto buffer = mk_buffer_from_char(data, len, nullptr, nullptr);
    mk_tcp_session_send_buffer_safe(ctx, buffer);
    mk_buffer_unref(buffer);
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
        s_events_server.on_mk_tcp_session_data(_local_port, this, (mk_buffer)&buffer);
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
                //此处你也可以修改WebSocketHeader::BINARY
                s_tcp_server[type]->start<WebSocketSession<TcpSessionForC, HttpSession, WebSocketHeader::TEXT> >(port);
                break;
            case mk_type_wss:
                //此处你也可以修改WebSocketHeader::BINARY
                s_tcp_server[type]->start<WebSocketSession<TcpSessionForC, HttpsSession, WebSocketHeader::TEXT> >(port);
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
    if (_events.on_mk_tcp_client_data) {
        _events.on_mk_tcp_client_data(_client, (mk_buffer)&pBuf);
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
            return (TcpClientForC::Ptr *)new std::shared_ptr<TcpSessionWithSSL<TcpClientForC> >(new TcpSessionWithSSL<TcpClientForC>(events));
        case mk_type_ws:
            //此处你也可以修改WebSocketHeader::BINARY
            return (TcpClientForC::Ptr *)new std::shared_ptr<WebSocketClient<TcpClientForC, WebSocketHeader::TEXT, false> >(new WebSocketClient<TcpClientForC, WebSocketHeader::TEXT, false>(events));
        case mk_type_wss:
            //此处你也可以修改WebSocketHeader::BINARY
            return (TcpClientForC::Ptr *)new std::shared_ptr<WebSocketClient<TcpClientForC, WebSocketHeader::TEXT, true> >(new WebSocketClient<TcpClientForC, WebSocketHeader::TEXT, true>(events));
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

API_EXPORT void API_CALL mk_tcp_client_send_buffer(mk_tcp_client ctx, mk_buffer buffer) {
    assert(ctx && buffer);
    TcpClientForC::Ptr *client = (TcpClientForC::Ptr *) ctx;
    (*client)->send(*((Buffer::Ptr *) buffer));
}

API_EXPORT void API_CALL mk_tcp_client_send(mk_tcp_client ctx, const char *data, int len) {
    auto buffer = mk_buffer_from_char(data, len, nullptr, nullptr);
    mk_tcp_client_send_buffer(ctx, buffer);
    mk_buffer_unref(buffer);
}

API_EXPORT void API_CALL mk_tcp_client_send_buffer_safe(mk_tcp_client ctx, mk_buffer buffer) {
    assert(ctx && buffer);
    TcpClientForC::Ptr *client = (TcpClientForC::Ptr *) ctx;
    std::weak_ptr<TcpClient> weakClient = *client;
    auto ref = mk_buffer_ref(buffer);
    (*client)->async([weakClient, ref]() {
        auto strongClient = weakClient.lock();
        if (strongClient) {
            strongClient->send(*((Buffer::Ptr *) ref));
        }
        mk_buffer_unref(ref);
    });
}

API_EXPORT void API_CALL mk_tcp_client_send_safe(mk_tcp_client ctx, const char *data, int len){
    auto buffer = mk_buffer_from_char(data, len, nullptr, nullptr);
    mk_tcp_client_send_buffer_safe(ctx, buffer);
    mk_buffer_unref(buffer);
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
