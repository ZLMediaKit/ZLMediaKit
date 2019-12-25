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

#ifndef MK_WEBSOCKET_H
#define MK_WEBSOCKET_H

#include "common.h"
#include "events_objects.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    /**
     * 当websocket客户端连接服务器时触发
     * @param session 会话处理对象
     */
    void (API_CALL *on_mk_websocket_session_create)(mk_tcp_session session);

    /**
     * session会话对象销毁时触发
     * 请在本回调中清理释放你的用户数据
     * 本事件中不能调用mk_tcp_session_send/mk_tcp_session_send_safe函数
     * @param session 会话处理对象
     */
    void (API_CALL *on_mk_websocket_session_destory)(mk_tcp_session session);

    /**
     * 收到websocket客户端发过来的数据
     * @param session 会话处理对象
     * @param data 数据指针
     * @param len 数据长度
     */
    void (API_CALL *on_mk_websocket_session_data)(mk_tcp_session session,const char *data,int len);

    /**
     * 每隔2秒的定时器，用于管理超时等任务
     * @param session 会话处理对象
     */
    void (API_CALL *on_mk_websocket_session_manager)(mk_tcp_session session);

    /**
     * on_mk_websocket_session_destory之前触发on_mk_websocket_session_err
     * 一般由于客户端断开tcp触发
     * 本事件中可以调用mk_tcp_session_send_safe函数
     * @param session 会话处理对象
     * @param code 错误代码
     * @param msg 错误提示
     */
    void (API_CALL *on_mk_websocket_session_err)(mk_tcp_session session,int code,const char *msg);
} mk_websocket_events;

API_EXPORT void API_CALL mk_websocket_events_listen(const mk_websocket_events *events);

/**
 * 往websocket会话对象附着用户数据
 * @param session websocket会话对象
 * @param user_data 用户数据指针
 */
API_EXPORT void API_CALL mk_websocket_session_set_user_data(mk_tcp_session session,void *user_data);

/**
 * 获取websocket会话对象上附着的用户数据
 * @param session websocket会话对象
 * @return 用户数据指针
 */
API_EXPORT void* API_CALL mk_websocket_session_get_user_data(mk_tcp_session session,void *user_data);

/**
 * 开启websocket服务器,需要指出的是，websocket服务器包含了Http服务器的所有功能
 * 调用mk_websocket_server_start后不用再调用mk_http_server_start
 * @param port 端口号，0则随机
 * @param ssl 是否为wss/ws
 * @return 端口号，0代表失败
 */
API_EXPORT uint16_t API_CALL mk_websocket_server_start(uint16_t port, int ssl);

#ifdef __cplusplus
}
#endif
#endif //MK_WEBSOCKET_H
