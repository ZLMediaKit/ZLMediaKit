/*
 * Copyright (c) 2016-present The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/ZLMediaKit/ZLMediaKit).
 *
 * Use of this source code is governed by MIT-like license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MK_PLAYER_H_
#define MK_PLAYER_H_

#include "mk_common.h"
#include "mk_frame.h"
#include "mk_track.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct mk_player_t *mk_player;

/**
 * 播放结果或播放中断事件的回调
 * @param user_data 用户数据指针
 * @param err_code 错误代码，0为成功
 * @param err_msg 错误提示
 * @param tracks track列表
 * @param track_count track个数
 * Callback for playback result or playback interruption event
 * @param user_data User data pointer
 * @param err_code Error code, 0 for success
 * @param err_msg Error message
 * @param tracks Track list
 * @param track_count Number of tracks
 
 * [AUTO-TRANSLATED:38d4c546]
 */
typedef void(API_CALL *on_mk_play_event)(void *user_data, int err_code, const char *err_msg, mk_track tracks[],
                                         int track_count);

/**
 * 创建一个播放器,支持rtmp[s]/rtsp[s]
 * @return 播放器指针
 * Create a player that supports rtmp[s]/rtsp[s]
 * @return Player pointer
 
 * [AUTO-TRANSLATED:509f9a50]
 */
API_EXPORT mk_player API_CALL mk_player_create();

/**
 * 销毁播放器
 * @param ctx 播放器指针
 * Destroy the player
 * @param ctx Player pointer
 
 * [AUTO-TRANSLATED:2448eb93]
 */
API_EXPORT void API_CALL mk_player_release(mk_player ctx);

/**
 * 设置播放器配置选项
 * @param ctx 播放器指针
 * @param key 配置项键,支持 net_adapter/rtp_type/rtsp_user/rtsp_pwd/protocol_timeout_ms/media_timeout_ms/beat_interval_ms/wait_track_ready
 * @param val 配置项值,如果是整形，需要转换成统一转换成string
 * Set player configuration options
 * @param ctx Player pointer
 * @param key Configuration key, supports net_adapter/rtp_type/rtsp_user/rtsp_pwd/protocol_timeout_ms/media_timeout_ms/beat_interval_ms/wait_track_ready
 * @param val Configuration value, if it is an integer, it needs to be converted to a string
 
 * [AUTO-TRANSLATED:12650e9f]
 */
API_EXPORT void API_CALL mk_player_set_option(mk_player ctx, const char *key, const char *val);

/**
 * 开始播放url
 * @param ctx 播放器指针
 * @param url rtsp[s]/rtmp[s] url
 * Start playing the url
 * @param ctx Player pointer
 * @param url rtsp[s]/rtmp[s] url
 
 * [AUTO-TRANSLATED:dbec813f]
 */
API_EXPORT void API_CALL mk_player_play(mk_player ctx, const char *url);

/**
 * 暂停或恢复播放，仅对点播有用
 * @param ctx 播放器指针
 * @param pause 1:暂停播放，0：恢复播放
 * Pause or resume playback, only useful for on-demand
 * @param ctx Player pointer
 * @param pause 1: Pause playback, 0: Resume playback
 
 * [AUTO-TRANSLATED:28eee990]
 */
API_EXPORT void API_CALL mk_player_pause(mk_player ctx, int pause);

/**
 * 倍数播放，仅对点播有用
 * @param ctx 播放器指针
 * @param speed 0.5 1.0 2.0 
 * Playback at a multiple, only useful for on-demand
 * @param ctx Player pointer
 * @param speed 0.5 1.0 2.0
 
 * [AUTO-TRANSLATED:95249ade]
 */
API_EXPORT void API_CALL mk_player_speed(mk_player ctx, float speed);

/**
 * 设置点播进度条
 * @param ctx 对象指针
 * @param progress 取值范围未 0.0～1.0
 * Set the on-demand progress bar
 * @param ctx Object pointer
 * @param progress Value range is 0.0～1.0
 
 * [AUTO-TRANSLATED:cede3a8f]
 */
API_EXPORT void API_CALL mk_player_seekto(mk_player ctx, float progress);

/**
 * 设置点播进度条
 * @param ctx 对象指针
 * @param seek_pos 取值范围 相对于开始时间增量 单位秒
 * Set the on-demand progress bar
 * @param ctx Object pointer
 * @param seek_pos Value range is the increment relative to the start time, unit is seconds
 
 * [AUTO-TRANSLATED:cddea627]
 */
API_EXPORT void API_CALL mk_player_seekto_pos(mk_player ctx, int seek_pos);

/**
 * 设置播放器开启播放结果回调函数
 * @param ctx 播放器指针
 * @param cb 回调函数指针,设置null立即取消回调
 * @param user_data 用户数据指针
 * Set the player to enable playback result callback function
 * @param ctx Player pointer
 * @param cb Callback function pointer, set null to immediately cancel the callback
 * @param user_data User data pointer
 
 * [AUTO-TRANSLATED:1c2daeaf]
 */
API_EXPORT void API_CALL mk_player_set_on_result(mk_player ctx, on_mk_play_event cb, void *user_data);
API_EXPORT void API_CALL mk_player_set_on_result2(mk_player ctx, on_mk_play_event cb, void *user_data, on_user_data_free user_data_free);

/**
 * 设置播放被异常中断的回调
 * @param ctx 播放器指针
 * @param cb 回调函数指针,设置null立即取消回调
 * @param user_data 用户数据指针
 * Set the callback for playback being abnormally interrupted
 * @param ctx Player pointer
 * @param cb Callback function pointer, set null to immediately cancel the callback
 * @param user_data User data pointer
 
 ///////////////////////////Audio and video related information interfaces are only valid after the playback success callback is triggered///////////////////////////////
 * [AUTO-TRANSLATED:18f58697]
 */
API_EXPORT void API_CALL mk_player_set_on_shutdown(mk_player ctx, on_mk_play_event cb, void *user_data);
API_EXPORT void API_CALL mk_player_set_on_shutdown2(mk_player ctx, on_mk_play_event cb, void *user_data, on_user_data_free user_data_free);

// /////////////////////////获取音视频相关信息接口在播放成功回调触发后才有效///////////////////////////////  [AUTO-TRANSLATED:4b53d8ff]
// * Get the duration of the on-demand program, if it is live, return 0, otherwise return the number of seconds

/**
 * 获取点播节目时长，如果是直播返回0，否则返回秒数
 * Get the on-demand playback progress, value range is 0.0～1.0
 
 * [AUTO-TRANSLATED:522140b7]
 */
API_EXPORT float API_CALL mk_player_duration(mk_player ctx);

/**
 * 获取点播播放进度，取值范围 0.0～1.0
 * Get the on-demand playback progress position, value range is the increment relative to the start time, unit is seconds
 
 * [AUTO-TRANSLATED:921795a0]
 */
API_EXPORT float API_CALL mk_player_progress(mk_player ctx);

/**
 * 获取点播播放进度位置，取值范围 相对于开始时间增量 单位秒
 * Get the packet loss rate, valid for rtsp
 * @param ctx Object pointer
 * @param track_type 0: Video, 1: Audio
 
 * [AUTO-TRANSLATED:058e5089]
 */
API_EXPORT int API_CALL mk_player_progress_pos(mk_player ctx);

/**
 * 获取丢包率，rtsp时有效
 * @param ctx 对象指针
 * @param track_type 0：视频，1：音频
 */
API_EXPORT float API_CALL mk_player_loss_rate(mk_player ctx, int track_type);

#ifdef __cplusplus
}
#endif

#endif /* MK_PLAYER_H_ */
