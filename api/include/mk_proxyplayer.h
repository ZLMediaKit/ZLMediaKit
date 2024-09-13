/*
 * Copyright (c) 2016-present The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/ZLMediaKit/ZLMediaKit).
 *
 * Use of this source code is governed by MIT-like license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MK_PROXY_PLAYER_H_
#define MK_PROXY_PLAYER_H_

#include "mk_common.h"
#include "mk_util.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct mk_proxy_player_t *mk_proxy_player;

/**
 * 创建一个代理播放器
 * @param vhost 虚拟主机名，一般为__defaultVhost__
 * @param app 应用名
 * @param stream 流名
 * @param rtp_type rtsp播放方式:RTP_TCP = 0, RTP_UDP = 1, RTP_MULTICAST = 2
 * @param hls_enabled 是否生成hls
 * @param mp4_enabled 是否生成mp4
 * @return 对象指针
 * Create a proxy player
 * @param vhost Virtual host name, generally __defaultVhost__
 * @param app Application name
 * @param stream Stream name
 * @param rtp_type rtsp playback method: RTP_TCP = 0, RTP_UDP = 1, RTP_MULTICAST = 2
 * @param hls_enabled Whether to generate hls
 * @param mp4_enabled Whether to generate mp4
 * @return Object pointer
 
 * [AUTO-TRANSLATED:1d4f13f4]
 */
API_EXPORT mk_proxy_player API_CALL mk_proxy_player_create(const char *vhost, const char *app, const char *stream, int hls_enabled, int mp4_enabled);


/**
 * 创建一个代理播放器
 * @param vhost 虚拟主机名，一般为__defaultVhost__
 * @param app 应用名
 * @param stream 流名
 * @param option ProtocolOption相关配置
 * @return 对象指针
 * Create a proxy player
 * @param vhost Virtual host name, generally __defaultVhost__
 * @param app Application name
 * @param stream Stream name
 * @param option ProtocolOption related configuration
 * @return Object pointer
 
 * [AUTO-TRANSLATED:4c6208df]
 */
API_EXPORT mk_proxy_player API_CALL mk_proxy_player_create2(const char *vhost, const char *app, const char *stream, mk_ini option);


/**
 * 创建一个代理播放器
 * @param vhost 虚拟主机名，一般为__defaultVhost__
 * @param app 应用名
 * @param stream 流名
 * @param rtp_type rtsp播放方式:RTP_TCP = 0, RTP_UDP = 1, RTP_MULTICAST = 2
 * @param hls_enabled 是否生成hls
 * @param mp4_enabled 是否生成mp4
 * @param retry_count 重试次数，当<0无限次重试
 * @return 对象指针
 * Create a proxy player
 * @param vhost Virtual host name, generally __defaultVhost__
 * @param app Application name
 * @param stream Stream name
 * @param rtp_type rtsp playback method: RTP_TCP = 0, RTP_UDP = 1, RTP_MULTICAST = 2
 * @param hls_enabled Whether to generate hls
 * @param mp4_enabled Whether to generate mp4
 * @param retry_count Retry count, when <0 retry infinitely
 * @return Object pointer
 
 * [AUTO-TRANSLATED:e25286c3]
 */
API_EXPORT mk_proxy_player API_CALL mk_proxy_player_create3(const char *vhost, const char *app, const char *stream, int hls_enabled, int mp4_enabled, int retry_count);


/**
 * 创建一个代理播放器
 * @param vhost 虚拟主机名，一般为__defaultVhost__
 * @param app 应用名
 * @param stream 流名
 * @param option ProtocolOption相关配置
 * @param retry_count 重试次数，当<0无限次重试
 * @return 对象指针
 * Create a proxy player
 * @param vhost Virtual host name, generally __defaultVhost__
 * @param app Application name
 * @param stream Stream name
 * @param option ProtocolOption related configuration
 * @param retry_count Retry count, when <0 retry infinitely
 * @return Object pointer
 
 * [AUTO-TRANSLATED:2cb296d1]
 */
API_EXPORT mk_proxy_player API_CALL mk_proxy_player_create4(const char *vhost, const char *app, const char *stream, mk_ini option, int retry_count);


/**
 * 销毁代理播放器
 * @param ctx 对象指针
 * Destroy the proxy player
 * @param ctx Object pointer
 
 * [AUTO-TRANSLATED:fe451691]
 */
API_EXPORT void API_CALL mk_proxy_player_release(mk_proxy_player ctx);

/**
 * 设置代理播放器配置选项
 * @param ctx 代理播放器指针
 * @param key 配置项键,支持 net_adapter/rtp_type/rtsp_user/rtsp_pwd/protocol_timeout_ms/media_timeout_ms/beat_interval_ms/rtsp_speed
 * @param val 配置项值,如果是整形，需要转换成统一转换成string
 * Set proxy player configuration options
 * @param ctx Proxy player pointer
 * @param key Configuration item key, supports net_adapter/rtp_type/rtsp_user/rtsp_pwd/protocol_timeout_ms/media_timeout_ms/beat_interval_ms/rtsp_speed
 * @param val Configuration item value, if it is an integer, it needs to be converted to a unified string
 
 * [AUTO-TRANSLATED:78938fba]
 */
API_EXPORT void API_CALL mk_proxy_player_set_option(mk_proxy_player ctx, const char *key, const char *val);

/**
 * 开始播放
 * @param ctx 对象指针
 * @param url 播放url,支持rtsp/rtmp
 * Start playback
 * @param ctx Object pointer
 * @param url Playback url, supports rtsp/rtmp
 
 * [AUTO-TRANSLATED:9597bafb]
 */
API_EXPORT void API_CALL mk_proxy_player_play(mk_proxy_player ctx, const char *url);

/**
 * MediaSource.close()回调事件
 * 在选择关闭一个关联的MediaSource时，将会最终触发到该回调
 * 你应该通过该事件调用mk_proxy_player_release函数并且释放其他资源
 * 如果你不调用mk_proxy_player_release函数，那么MediaSource.close()操作将无效
 * @param user_data 用户数据指针，通过mk_proxy_player_set_on_close函数设置
 * MediaSource.close() callback event
 * When you choose to close an associated MediaSource, it will eventually trigger this callback
 * You should call mk_proxy_player_release function through this event and release other resources
 * If you do not call mk_proxy_player_release function, then MediaSource.close() operation will be invalid
 * @param user_data User data pointer, set by mk_proxy_player_set_on_close function
 
 * [AUTO-TRANSLATED:c99b6bfd]
 */
typedef void(API_CALL *on_mk_proxy_player_cb)(void *user_data, int err, const char *what, int sys_err);
// 保持兼容  [AUTO-TRANSLATED:94139ca7]
// Keep compatible
#define on_mk_proxy_player_close on_mk_proxy_player_cb

/**
 * 监听MediaSource.close()事件
 * 在选择关闭一个关联的MediaSource时，将会最终触发到该回调
 * 你应该通过该事件调用mk_proxy_player_release函数并且释放其他资源
 * @param ctx 对象指针
 * @param cb 回调指针
 * @param user_data 用户数据指针
 * Listen for MediaSource.close() event
 * When you choose to close an associated MediaSource, it will eventually trigger this callback
 * You should call mk_proxy_player_release function through this event and release other resources
 * @param ctx Object pointer
 * @param cb Callback pointer
 * @param user_data User data pointer
 
 * [AUTO-TRANSLATED:174060d4]
 */
API_EXPORT void API_CALL mk_proxy_player_set_on_close(mk_proxy_player ctx, on_mk_proxy_player_cb cb, void *user_data);
API_EXPORT void API_CALL mk_proxy_player_set_on_close2(mk_proxy_player ctx, on_mk_proxy_player_cb cb, void *user_data, on_user_data_free user_data_free);

/**
 * 设置代理第一次播放结果回调，如果第一次播放失败，可以认作是启动失败
 * @param ctx 对象指针
 * @param cb 回调指针
 * @param user_data 用户数据指针
 * @param user_data_free 用户数据释放回调
 * Set the proxy's first playback result callback. If the first playback fails, it can be considered a startup failure.
 * @param ctx Object pointer
 * @param cb Callback pointer
 * @param user_data User data pointer
 * @param user_data_free User data release callback
 
 * [AUTO-TRANSLATED:1f34852a]
 */
API_EXPORT void API_CALL mk_proxy_player_set_on_play_result(mk_proxy_player ctx, on_mk_proxy_player_cb cb, void *user_data, on_user_data_free user_data_free);

/**
 * 获取总的观看人数
 * @param ctx 对象指针
 * @return 观看人数
 * Get the total number of viewers
 * @param ctx Object pointer
 * @return Number of viewers
 
 * [AUTO-TRANSLATED:56635caf]
 */
API_EXPORT int API_CALL mk_proxy_player_total_reader_count(mk_proxy_player ctx);

#ifdef __cplusplus
}
#endif

#endif /* MK_PROXY_PLAYER_H_ */
