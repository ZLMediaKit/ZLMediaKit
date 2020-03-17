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

#ifndef MK_TCP_H
#define MK_TCP_H

#include "mk_common.h"

#ifdef __cplusplus
extern "C" {
#endif

///////////////////////////////////////////TcpSession/////////////////////////////////////////////
//TcpSession对象的C映射
typedef void* mk_tcp_session;
//TcpSession::safeShutdown()
API_EXPORT void API_CALL mk_tcp_session_shutdown(const mk_tcp_session ctx,int err,const char *err_msg);
//TcpSession::get_peer_ip()
API_EXPORT const char* API_CALL mk_tcp_session_peer_ip(const mk_tcp_session ctx);
//TcpSession::get_local_ip()
API_EXPORT const char* API_CALL mk_tcp_session_local_ip(const mk_tcp_session ctx);
//TcpSession::get_peer_port()
API_EXPORT uint16_t API_CALL mk_tcp_session_peer_port(const mk_tcp_session ctx);
//TcpSession::get_local_port()
API_EXPORT uint16_t API_CALL mk_tcp_session_local_port(const mk_tcp_session ctx);
//TcpSession::send()
API_EXPORT void API_CALL mk_tcp_session_send(const mk_tcp_session ctx,const char *data,int len);
//切换到该对象所在线程后再TcpSession::send()
API_EXPORT void API_CALL mk_tcp_session_send_safe(const mk_tcp_session ctx,const char *data,int len);

///////////////////////////////////////////自定义tcp服务/////////////////////////////////////////////

typedef struct {
    /**
     * 收到mk_tcp_session创建对象
     * @param server_port 服务器端口号
     * @param session 会话处理对象
     */
    void (API_CALL *on_mk_tcp_session_create)(uint16_t server_port,mk_tcp_session session);

    /**
     * 收到客户端发过来的数据
     * @param server_port 服务器端口号
     * @param session 会话处理对象
     * @param data 数据指针
     * @param len 数据长度
     */
    void (API_CALL *on_mk_tcp_session_data)(uint16_t server_port,mk_tcp_session session,const char *data,int len);

    /**
     * 每隔2秒的定时器，用于管理超时等任务
     * @param server_port 服务器端口号
     * @param session 会话处理对象
     */
    void (API_CALL *on_mk_tcp_session_manager)(uint16_t server_port,mk_tcp_session session);

    /**
     * 一般由于客户端断开tcp触发
     * @param server_port 服务器端口号
     * @param session 会话处理对象
     * @param code 错误代码
     * @param msg 错误提示
     */
    void (API_CALL *on_mk_tcp_session_disconnect)(uint16_t server_port,mk_tcp_session session,int code,const char *msg);
} mk_tcp_session_events;


typedef enum {
    //普通的tcp
    mk_type_tcp = 0,
    //ssl类型的tcp
    mk_type_ssl = 1,
    //基于websocket的连接
    mk_type_ws = 2,
    //基于ssl websocket的连接
    mk_type_wss = 3
}mk_tcp_type;

/**
 * tcp会话对象附着用户数据
 * 该函数只对mk_tcp_server_server_start启动的服务类型有效
 * @param session 会话对象
 * @param user_data 用户数据指针
 */
API_EXPORT void API_CALL mk_tcp_session_set_user_data(mk_tcp_session session,void *user_data);

/**
 * 获取tcp会话对象上附着的用户数据
 * 该函数只对mk_tcp_server_server_start启动的服务类型有效
 * @param session tcp会话对象
 * @return 用户数据指针
 */
API_EXPORT void* API_CALL mk_tcp_session_get_user_data(mk_tcp_session session);

/**
 * 开启tcp服务器
 * @param port 监听端口号，0则为随机
 * @param type 服务器类型
 */
API_EXPORT uint16_t API_CALL mk_tcp_server_start(uint16_t port, mk_tcp_type type);

/**
 * 监听tcp服务器事件
 */
API_EXPORT void API_CALL mk_tcp_server_events_listen(const mk_tcp_session_events *events);


///////////////////////////////////////////自定义tcp客户端/////////////////////////////////////////////

typedef void* mk_tcp_client;

typedef struct {
    /**
     * tcp客户端连接服务器成功或失败回调
     * @param client tcp客户端
     * @param code 0为连接成功，否则为失败原因
     * @param msg 连接失败错误提示
     */
    void (API_CALL *on_mk_tcp_client_connect)(mk_tcp_client client,int code,const char *msg);

    /**
     * tcp客户端与tcp服务器之间断开回调
     * 一般是eof事件导致
     * @param client tcp客户端
     * @param code 错误代码
     * @param msg 错误提示
     */
    void (API_CALL *on_mk_tcp_client_disconnect)(mk_tcp_client client,int code,const char *msg);

    /**
     * 收到tcp服务器发来的数据
     * @param client tcp客户端
     * @param data 数据指针
     * @param len 数据长度
     */
    void (API_CALL *on_mk_tcp_client_data)(mk_tcp_client client,const char *data,int len);

    /**
     * 每隔2秒的定时器，用于管理超时等任务
     * @param client tcp客户端
     */
    void (API_CALL *on_mk_tcp_client_manager)(mk_tcp_client client);
} mk_tcp_client_events;

/**
 * 创建tcp客户端
 * @param events 回调函数结构体
 * @param user_data 用户数据指针
 * @param type 客户端类型
 * @return 客户端对象
 */
API_EXPORT mk_tcp_client API_CALL mk_tcp_client_create(mk_tcp_client_events *events, mk_tcp_type type);

/**
 * 释放tcp客户端
 * @param ctx 客户端对象
 */
API_EXPORT void API_CALL mk_tcp_client_release(mk_tcp_client ctx);

/**
 * 发起连接
 * @param ctx 客户端对象
 * @param host 服务器ip或域名
 * @param port 服务器端口号
 * @param time_out_sec 超时时间
 */
API_EXPORT void API_CALL mk_tcp_client_connect(mk_tcp_client ctx, const char *host, uint16_t port, float time_out_sec);

/**
 * 非线程安全的发送数据
 * 开发者如果能确保在本对象网络线程内，可以调用此此函数
 * @param ctx 客户端对象
 * @param data 数据指针
 * @param len 数据长度，等于0时，内部通过strlen获取
 */
API_EXPORT void API_CALL mk_tcp_client_send(mk_tcp_client ctx, const char *data, int len);

/**
 * 切换到本对象的网络线程后再发送数据
 * @param ctx 客户端对象
 * @param data 数据指针
 * @param len 数据长度，等于0时，内部通过strlen获取
 */
API_EXPORT void API_CALL mk_tcp_client_send_safe(mk_tcp_client ctx, const char *data, int len);

/**
 * 客户端附着用户数据
 * @param ctx 客户端对象
 * @param user_data 用户数据指针
 */
API_EXPORT void API_CALL mk_tcp_client_set_user_data(mk_tcp_client ctx,void *user_data);

/**
 * 获取客户端对象上附着的用户数据
 * @param ctx 客户端对象
 * @return 用户数据指针
 */
API_EXPORT void* API_CALL mk_tcp_client_get_user_data(mk_tcp_client ctx);

#ifdef __cplusplus
}
#endif
#endif //MK_TCP_H
