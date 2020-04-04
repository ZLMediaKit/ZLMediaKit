/*
 * Copyright (c) 2016 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/xiongziliang/ZLMediaKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MK_PUSHER_H
#define MK_PUSHER_H

#include "mk_common.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef void* mk_pusher;

/**
 * 推流结果或推流中断事件的回调
 * @param user_data 用户数据指针
 * @param err_code 错误代码，0为成功
 * @param err_msg 错误提示
 */
typedef void(API_CALL *on_mk_push_event)(void *user_data,int err_code,const char *err_msg);

/**
 * 绑定的MediaSource对象并创建rtmp[s]/rtsp[s]推流器
 * MediaSource通过mk_media_create或mk_proxy_player_create生成
 * 该MediaSource对象必须已注册
 *
 * @param schema 绑定的MediaSource对象所属协议，支持rtsp/rtmp
 * @param vhost 绑定的MediaSource对象的虚拟主机，一般为__defaultVhost__
 * @param app 绑定的MediaSource对象的应用名，一般为live
 * @param stream 绑定的MediaSource对象的流id
 * @return 对象指针
 */
API_EXPORT mk_pusher API_CALL mk_pusher_create(const char *schema,const char *vhost,const char *app, const char *stream);

/**
 * 释放推流器
 * @param ctx 推流器指针
 */
API_EXPORT void API_CALL mk_pusher_release(mk_pusher ctx);

/**
 * 设置推流器配置选项
 * @param ctx 推流器指针
 * @param key 配置项键,支持 net_adapter/rtp_type/rtsp_user/rtsp_pwd/protocol_timeout_ms/media_timeout_ms/beat_interval_ms/max_analysis_ms
 * @param val 配置项值,如果是整形，需要转换成统一转换成string
 */
API_EXPORT void API_CALL mk_pusher_set_option(mk_pusher ctx, const char *key, const char *val);

/**
 * 开始推流
 * @param ctx 推流器指针
 * @param url 推流地址，支持rtsp[s]/rtmp[s]
 */
API_EXPORT void API_CALL mk_pusher_publish(mk_pusher ctx,const char *url);

/**
 * 设置推流器推流结果回调函数
 * @param ctx 推流器指针
 * @param cb 回调函数指针,不得为null
 * @param user_data 用户数据指针
 */
API_EXPORT void API_CALL mk_pusher_set_on_result(mk_pusher ctx, on_mk_push_event cb, void *user_data);

/**
 * 设置推流被异常中断的回调
 * @param ctx 推流器指针
 * @param cb 回调函数指针,不得为null
 * @param user_data 用户数据指针
 */
API_EXPORT void API_CALL mk_pusher_set_on_shutdown(mk_pusher ctx, on_mk_push_event cb, void *user_data);

#ifdef __cplusplus
}
#endif
#endif //MK_PUSHER_H
