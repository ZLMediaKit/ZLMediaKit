/*
 * MIT License
 *
 * Copyright (c) 2019 xiongziliang <771730766@qq.com>
 *
 * This file is part of ZLMediaKit(https://github.com/xiongziliang/ZLMediaKit).
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include "websocket.h"
#include "Http/HttpSession.h"
#include "Http/WebSocketSession.h"
using namespace mediakit;

static TcpServer::Ptr websocket_server[2];
static mk_websocket_events s_events = {0};

class WebSocketSessionImp : public TcpSession {
public:
    WebSocketSessionImp(const Socket::Ptr &pSock) : TcpSession(pSock){
        if(s_events.on_mk_websocket_session_create){
            s_events.on_mk_websocket_session_create(this);
        }
    }
    
    virtual ~WebSocketSessionImp(){
        if(s_events.on_mk_websocket_session_destory){
            s_events.on_mk_websocket_session_destory(this);
        }
    }

    void onRecv(const Buffer::Ptr &buffer) override {
        if(s_events.on_mk_websocket_session_data){
            s_events.on_mk_websocket_session_data(this,buffer->data(),buffer->size());
        }
    }
    
    void onError(const SockException &err) override{
        if(s_events.on_mk_websocket_session_err){
            s_events.on_mk_websocket_session_err(this,err.getErrCode(),err.what());
        }
    }
    
    void onManager() override{
        if(s_events.on_mk_websocket_session_manager){
            s_events.on_mk_websocket_session_manager(this);
        }
    }

    void *_user_data;
};

API_EXPORT void API_CALL mk_websocket_events_listen(const mk_websocket_events *events){
    if(events){
        memcpy(&s_events,events, sizeof(s_events));
    }else{
        memset(&s_events,0,sizeof(s_events));
    }
}

API_EXPORT void API_CALL mk_websocket_session_set_user_data(mk_tcp_session session,void *user_data){
    assert(session);
    WebSocketSessionImp *obj = (WebSocketSessionImp *)session;
    obj->_user_data = user_data;
}

API_EXPORT void* API_CALL mk_websocket_session_get_user_data(mk_tcp_session session,void *user_data){
    assert(session);
    WebSocketSessionImp *obj = (WebSocketSessionImp *)session;
    return obj->_user_data;
}

API_EXPORT uint16_t API_CALL mk_websocket_server_start(uint16_t port, int ssl){
    ssl = MAX(0,MIN(ssl,1));
    try {
        websocket_server[ssl] = std::make_shared<TcpServer>();
        if(ssl){
            websocket_server[ssl]->start<WebSocketSession<WebSocketSessionImp,HttpsSession>>(port);
        }else{
            websocket_server[ssl]->start<WebSocketSession<WebSocketSessionImp,HttpSession>>(port);
        }
        return websocket_server[ssl]->getPort();
    } catch (std::exception &ex) {
        websocket_server[ssl].reset();
        WarnL << ex.what();
        return 0;
    }
}