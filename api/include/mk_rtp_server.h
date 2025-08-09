/*
 * Copyright (c) 2016-present The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/ZLMediaKit/ZLMediaKit).
 *
 * Use of this source code is governed by MIT-like license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#include "mk_common.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct mk_rtp_server_t *mk_rtp_server;

/**
 * 创建GB28181 RTP 服务器
 * @param port 监听端口，0则为随机
 * @param tcp_mode tcp模式(0: 不监听端口 1: 监听端口 2: 主动连接到服务端)
 * @param stream_id 该端口绑定的流id
 * @param multiple 多路复用RTP服务器 1: 开启 0: 不开启
 * @return
 * Create GB28181 RTP server
 * @param port Listening port, 0 for random
 * @param tcp_mode tcp mode (0: not listening to port 1: listening to port 2: actively connect to the server)
 * @param stream_id Stream id bound to this port
 * @return
 
 * [AUTO-TRANSLATED:0c5fd548]
 */
API_EXPORT mk_rtp_server API_CALL mk_rtp_server_create(uint16_t port, int tcp_mode, const char *stream_id);
API_EXPORT mk_rtp_server API_CALL mk_rtp_server_create2(uint16_t port, int tcp_mode, const char *vhost, const char *app, const char *stream_id);
API_EXPORT mk_rtp_server API_CALL mk_rtp_server_create3(uint16_t port, int tcp_mode, const char *vhost, const char *app, const char *stream_id, int multiplex);

/**
 * TCP 主动模式时连接到服务器是否成功的回调
 * Callback for whether the connection to the server is successful in TCP active mode
 
 * [AUTO-TRANSLATED:752e915a]
 */
typedef void(API_CALL *on_mk_rtp_server_connected)(void *user_data, int err, const char *what, int sys_err);

/**
 * TCP 主动模式时连接到服务器
 * @param @param ctx 服务器对象
 * @param dst_url 服务端地址
 * @param dst_port 服务端端口
 * @param cb 连接到服务器是否成功的回调
 * @param user_data 用户数据指针
 * @return
 * Connect to the server in TCP active mode
 * @param @param ctx Server object
 * @param dst_url Server address
 * @param dst_port Server port
 * @param cb Callback for whether the connection to the server is successful
 * @param user_data User data pointer
 * @return
 
 * [AUTO-TRANSLATED:e827d45a]
 */
API_EXPORT void API_CALL mk_rtp_server_connect(mk_rtp_server ctx, const char *dst_url, uint16_t dst_port, on_mk_rtp_server_connected cb, void *user_data);
API_EXPORT void API_CALL mk_rtp_server_connect2(mk_rtp_server ctx, const char *dst_url, uint16_t dst_port, on_mk_rtp_server_connected cb, void *user_data, on_user_data_free user_data_free);

/**
 * 销毁GB28181 RTP 服务器
 * @param ctx 服务器对象
 * Destroy GB28181 RTP server
 * @param ctx Server object
 
 * [AUTO-TRANSLATED:828e02f0]
 */
API_EXPORT void API_CALL mk_rtp_server_release(mk_rtp_server ctx);

/**
 * 获取本地监听的端口号
 * @param ctx 服务器对象
 * @return 端口号
 * Get the local listening port number
 * @param ctx Server object
 * @return Port number
 
 * [AUTO-TRANSLATED:90fe5d22]
 */
API_EXPORT uint16_t API_CALL mk_rtp_server_port(mk_rtp_server ctx);

/**
 * GB28181 RTP 服务器接收流超时时触发
 * @param user_data 用户数据指针
 * Triggered when the GB28181 RTP server receives a stream timeout
 * @param user_data User data pointer
 
 * [AUTO-TRANSLATED:04d56f24]
 */
typedef void(API_CALL *on_mk_rtp_server_detach)(void *user_data);

/**
 * 监听B28181 RTP 服务器接收流超时事件
 * @param ctx 服务器对象
 * @param cb 回调函数
 * @param user_data 回调函数用户数据指针
 * Listen for B28181 RTP server receiving stream timeout events
 * @param ctx Server object
 * @param cb Callback function
 * @param user_data Callback function user data pointer
 
 
 * [AUTO-TRANSLATED:a88c239f]
 */
API_EXPORT void API_CALL mk_rtp_server_set_on_detach(mk_rtp_server ctx, on_mk_rtp_server_detach cb, void *user_data);
API_EXPORT void API_CALL mk_rtp_server_set_on_detach2(mk_rtp_server ctx, on_mk_rtp_server_detach cb, void *user_data, on_user_data_free user_data_free);

/**
 * 更新RTP服务器过滤SSRC
 * @param ctx 服务器对象
 * @param ssrc 十进制ssrc
 * 
 */
API_EXPORT void API_CALL mk_rtp_server_update_ssrc(mk_rtp_server ctx, uint32_t ssrc);


/**
 * rtp信息获取回调
 * @param exist 存在rtp信息 0: 不存在 1: 存在
 * @param peer_ip 连接ip
 * @param peer_port 连接端口
 * @param local_ip 本地ip
 * @param local_port 本地端口
 * @param identifier 身份信息
 * 
 */
typedef void(API_CALL *on_mk_rtp_get_info)(int exist, const char *peer_ip, uint16_t peer_port, const char *local_ip, uint16_t local_port, const char *identifier);

/**
 *  获取rtp推流信息
 * @param app 应用名
 * @param stream 流id
 * @param cb rtp信息获取回调
 *
 */
API_EXPORT void API_CALL mk_rtp_get_info(const char *app, const char *stream, on_mk_rtp_get_info cb);


/**
 * 暂停RTP超时检查
 * @param app 应用名
 * @param stream 流id
 *
 */
API_EXPORT void API_CALL mk_rtp_pause_check(const char *app, const char *stream);

/**
 * 恢复RTP超时检查
 * @param app 应用名
 * @param stream 流id
 *
 */
API_EXPORT void API_CALL mk_rtp_resume_check(const char *app, const char *stream);

#ifdef __cplusplus
}
#endif