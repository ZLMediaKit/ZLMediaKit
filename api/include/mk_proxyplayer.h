/*
 * Copyright (c) 2016 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/xiongziliang/ZLMediaKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MK_PROXY_PLAYER_H_
#define MK_PROXY_PLAYER_H_

#include "mk_common.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef void *mk_proxy_player;

/**
 * 创建一个代理播放器
 * @param vhost 虚拟主机名，一般为__defaultVhost__
 * @param app 应用名
 * @param stream 流名
 * @param rtp_type rtsp播放方式:RTP_TCP = 0, RTP_UDP = 1, RTP_MULTICAST = 2
 * @param hls_enabled 是否生成hls
 * @param mp4_enabled 是否生成mp4
 * @return 对象指针
 */
API_EXPORT mk_proxy_player API_CALL mk_proxy_player_create(const char *vhost, const char *app, const char *stream, int hls_enabled, int mp4_enabled);

/**
 * 销毁代理播放器
 * @param ctx 对象指针
 */
API_EXPORT void API_CALL mk_proxy_player_release(mk_proxy_player ctx);

/**
 * 设置代理播放器配置选项
 * @param ctx 代理播放器指针
 * @param key 配置项键,支持 net_adapter/rtp_type/rtsp_user/rtsp_pwd/protocol_timeout_ms/media_timeout_ms/beat_interval_ms/max_analysis_ms
 * @param val 配置项值,如果是整形，需要转换成统一转换成string
 */
API_EXPORT void API_CALL mk_proxy_player_set_option(mk_proxy_player ctx, const char *key, const char *val);

/**
 * 开始播放
 * @param ctx 对象指针
 * @param url 播放url,支持rtsp/rtmp
 */
API_EXPORT void API_CALL mk_proxy_player_play(mk_proxy_player ctx, const char *url);

/**
 * MediaSource.close()回调事件
 * 在选择关闭一个关联的MediaSource时，将会最终触发到该回调
 * 你应该通过该事件调用mk_proxy_player_release函数并且释放其他资源
 * 如果你不调用mk_proxy_player_release函数，那么MediaSource.close()操作将无效
 * @param user_data 用户数据指针，通过mk_proxy_player_set_on_close函数设置
 */
typedef void(API_CALL *on_mk_proxy_player_close)(void *user_data);

/**
 * 监听MediaSource.close()事件
 * 在选择关闭一个关联的MediaSource时，将会最终触发到该回调
 * 你应该通过该事件调用mk_proxy_player_release函数并且释放其他资源
 * @param ctx 对象指针
 * @param cb 回调指针
 * @param user_data 用户数据指针
 */
API_EXPORT void API_CALL mk_proxy_player_set_on_close(mk_proxy_player ctx, on_mk_proxy_player_close cb, void *user_data);

/**
 * 获取总的观看人数
 * @param ctx 对象指针
 * @return 观看人数
 */
API_EXPORT int API_CALL mk_proxy_player_total_reader_count(mk_proxy_player ctx);

#ifdef __cplusplus
}
#endif

#endif /* MK_PROXY_PLAYER_H_ */
