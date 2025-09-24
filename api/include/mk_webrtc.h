/*
 * Copyright (c) 2016-present The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/ZLMediaKit/ZLMediaKit).
 *
 * Use of this source code is governed by MIT-like license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MK_WEBRTC_H
#define MK_WEBRTC_H
#include "mk_common.h"
#include "mk_proxyplayer.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif


// 获取webrtc answer sdp回调函数  [AUTO-TRANSLATED:10c93fa9]
// Get webrtc answer sdp callback function
typedef void(API_CALL *on_mk_webrtc_get_answer_sdp)(void *user_data, const char *answer, const char *err);

// 获取webrtc proxy player信息回调函数
typedef void(API_CALL *on_mk_webrtc_get_proxy_player_info_cb)(const char *info_json, const char *err);

//WebRTC-注册到信令服务器、WebRTC-从信令服务器注销回调函数
typedef void(API_CALL *on_mk_webrtc_room_keeper_info_cb)(void *user_data, const char *room_key, const char *err);

//获取WebRTC-Peer查看注册信息、WebRTC-信令服务器查看注册信息回调函数
typedef void(API_CALL *on_mk_webrtc_room_keeper_data_cb)(const char *data);


/**
 * webrtc交换sdp，根据offer sdp生成answer sdp
 * @param user_data 回调用户指针
 * @param cb 回调函数
 * @param type webrtc插件类型，支持echo,play,push
 * @param offer webrtc offer sdp
 * @param url rtc url, 例如 rtc://__defaultVhost/app/stream?key1=val1&key2=val2
 * webrtc exchange sdp, generate answer sdp based on offer sdp
 * @param user_data Callback user pointer
 * @param cb Callback function
 * @param type webrtc plugin type, supports echo, play, push
 * @param offer webrtc offer sdp
 * @param url rtc url, for example rtc://__defaultVhost/app/stream?key1=val1&key2=val2

 * [AUTO-TRANSLATED:ea79659b]
 */
API_EXPORT void API_CALL mk_webrtc_get_answer_sdp(void *user_data, on_mk_webrtc_get_answer_sdp cb, const char *type, const char *offer, const char *url);

API_EXPORT void API_CALL mk_webrtc_get_answer_sdp2(
    void *user_data, on_user_data_free user_data_free, on_mk_webrtc_get_answer_sdp cb, const char *type, const char *offer, const char *url);

/**
 * 获取webrtc proxy player信息
 * @param mk_proxy_player 代理
 * @param cb 回调函数
 */
API_EXPORT void API_CALL mk_webrtc_get_proxy_player_info(mk_proxy_player ctx, on_mk_webrtc_get_proxy_player_info_cb cb);


/**
 * WebRTC-注册到信令服务器
 * @param server_host 信令服务器host
 * @param server_port 信令服务器port
 * @param room_id 房间id
 * @param ssl 是否启用ssl
 * @param cb 回调函数
 * @param user_data 用户数据
 */
API_EXPORT void API_CALL
mk_webrtc_add_room_keeper(const char *room_id, const char *server_host, uint16_t server_port, int ssl, on_mk_webrtc_room_keeper_info_cb cb, void *user_data);


API_EXPORT void API_CALL mk_webrtc_add_room_keeper2(
    const char *room_id, const char *server_host, uint16_t server_port, int ssl, on_mk_webrtc_room_keeper_info_cb cb, void *user_data,
    on_user_data_free user_data_free);


/**
 * WebRTC-从信令服务器注销
 * @param room_key 房间key
 * @param cb 回调函数
 * @param user_data 用户数据
 */
API_EXPORT void API_CALL mk_webrtc_del_room_keeper(const char *room_key, on_mk_webrtc_room_keeper_info_cb cb, void *user_data);

API_EXPORT void API_CALL
mk_webrtc_del_room_keeper2(const char *room_key, on_mk_webrtc_room_keeper_info_cb cb, void *user_data, on_user_data_free user_data_free);


/**
 * WebRTC-Peer查看注册信息
 * @param cb 回调函数
 */
API_EXPORT void API_CALL mk_webrtc_list_room_keeper(on_mk_webrtc_room_keeper_data_cb cb);

/**
 * WebRTC-信令服务器查看注册信息
 * @param cb 回调函数
 */
API_EXPORT void API_CALL mk_webrtc_list_rooms(on_mk_webrtc_room_keeper_data_cb cb);

#ifdef __cplusplus
}
#endif

#endif /* MK_WEBRTC_H */