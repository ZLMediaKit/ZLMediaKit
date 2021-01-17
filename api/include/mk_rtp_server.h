/*
 * Copyright (c) 2016 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/xia-chu/ZLMediaKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#include "mk_common.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef void* mk_rtp_server;

/**
 * 创建GB28181 RTP 服务器
 * @param port 监听端口，0则为随机
 * @param enable_tcp 创建udp端口时是否同时监听tcp端口
 * @param stream_id 该端口绑定的流id
 * @return
 */
API_EXPORT mk_rtp_server API_CALL mk_rtp_server_create(uint16_t port, int enable_tcp, const char *stream_id);

/**
 * 销毁GB28181 RTP 服务器
 * @param ctx 服务器对象
 */
API_EXPORT void API_CALL mk_rtp_server_release(mk_rtp_server ctx);

/**
 * 获取本地监听的端口号
 * @param ctx 服务器对象
 * @return 端口号
 */
API_EXPORT uint16_t API_CALL mk_rtp_server_port(mk_rtp_server ctx);

/**
 * GB28181 RTP 服务器接收流超时时触发
 * @param user_data 用户数据指针
 */
typedef void(API_CALL *on_mk_rtp_server_detach)(void *user_data);

/**
 * 监听B28181 RTP 服务器接收流超时事件
 * @param ctx 服务器对象
 * @param cb 回调函数
 * @param user_data 回调函数用户数据指针
 */
API_EXPORT void API_CALL mk_rtp_server_set_on_detach(mk_rtp_server ctx, on_mk_rtp_server_detach cb, void *user_data);


#ifdef __cplusplus
}
#endif