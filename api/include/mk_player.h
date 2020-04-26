/*
 * Copyright (c) 2016 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/xiongziliang/ZLMediaKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MK_PLAYER_H_
#define MK_PLAYER_H_

#include "mk_common.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef void* mk_player;

/**
 * 播放结果或播放中断事件的回调
 * @param user_data 用户数据指针
 * @param err_code 错误代码，0为成功
 * @param err_msg 错误提示
 */
typedef void(API_CALL *on_mk_play_event)(void *user_data,int err_code,const char *err_msg);

/**
 * 收到音视频数据回调
 * @param user_data 用户数据指针
 * @param track_type 0：视频，1：音频
 * @param codec_id 0：H264，1：H265，2：AAC 3.G711A 4.G711U
 * @param data 数据指针
 * @param len 数据长度
 * @param dts 解码时间戳，单位毫秒
 * @param pts 显示时间戳，单位毫秒
 */
typedef void(API_CALL *on_mk_play_data)(void *user_data,int track_type,int codec_id,void *data,int len,uint32_t dts,uint32_t pts);

/**
 * 创建一个播放器,支持rtmp[s]/rtsp[s]
 * @return 播放器指针
 */
API_EXPORT mk_player API_CALL mk_player_create();

/**
 * 销毁播放器
 * @param ctx 播放器指针
 */
API_EXPORT void API_CALL mk_player_release(mk_player ctx);

/**
 * 设置播放器配置选项
 * @param ctx 播放器指针
 * @param key 配置项键,支持 net_adapter/rtp_type/rtsp_user/rtsp_pwd/protocol_timeout_ms/media_timeout_ms/beat_interval_ms/max_analysis_ms
 * @param val 配置项值,如果是整形，需要转换成统一转换成string
 */
API_EXPORT void API_CALL mk_player_set_option(mk_player ctx, const char *key, const char *val);

/**
 * 开始播放url
 * @param ctx 播放器指针
 * @param url rtsp[s]/rtmp[s] url
 */
API_EXPORT void API_CALL mk_player_play(mk_player ctx, const char *url);

/**
 * 暂停或恢复播放，仅对点播有用
 * @param ctx 播放器指针
 * @param pause 1:暂停播放，0：恢复播放
 */
API_EXPORT void API_CALL mk_player_pause(mk_player ctx, int pause);

/**
 * 设置点播进度条
 * @param ctx 对象指针
 * @param progress 取值范围未 0.0～1.0
 */
API_EXPORT void API_CALL mk_player_seekto(mk_player ctx, float progress);

/**
 * 设置播放器开启播放结果回调函数
 * @param ctx 播放器指针
 * @param cb 回调函数指针,设置null立即取消回调
 * @param user_data 用户数据指针
 */
API_EXPORT void API_CALL mk_player_set_on_result(mk_player ctx, on_mk_play_event cb, void *user_data);

/**
 * 设置播放被异常中断的回调
 * @param ctx 播放器指针
 * @param cb 回调函数指针,设置null立即取消回调
 * @param user_data 用户数据指针
 */
API_EXPORT void API_CALL mk_player_set_on_shutdown(mk_player ctx, on_mk_play_event cb, void *user_data);

/**
 * 设置音视频数据回调函数
 * @param ctx 播放器指针
 * @param cb 回调函数指针,设置null立即取消回调
 * @param user_data 用户数据指针
 */
API_EXPORT void API_CALL mk_player_set_on_data(mk_player ctx, on_mk_play_data cb, void *user_data);

///////////////////////////获取音视频相关信息接口在播放成功回调触发后才有效///////////////////////////////

/**
 * 获取视频codec_id -1：不存在 0：H264，1：H265，2：AAC 3.G711A 4.G711U
 * @param ctx 播放器指针
 */
API_EXPORT int API_CALL mk_player_video_codecId(mk_player ctx);

/**
 * 获取视频宽度
 */
API_EXPORT int API_CALL mk_player_video_width(mk_player ctx);

/**
 * 获取视频高度
 */
API_EXPORT int API_CALL mk_player_video_height(mk_player ctx);

/**
 * 获取视频帧率
 */
API_EXPORT int API_CALL mk_player_video_fps(mk_player ctx);

/**
 * 获取音频codec_id -1：不存在 0：H264，1：H265，2：AAC 3.G711A 4.G711U
 * @param ctx 播放器指针
 */
API_EXPORT int API_CALL mk_player_audio_codecId(mk_player ctx);

/**
 * 获取音频采样率
 */
API_EXPORT int API_CALL mk_player_audio_samplerate(mk_player ctx);

/**
 * 获取音频采样位数，一般为16
 */
API_EXPORT int API_CALL mk_player_audio_bit(mk_player ctx);

/**
 * 获取音频通道数
 */
API_EXPORT int API_CALL mk_player_audio_channel(mk_player ctx);

/**
 * 获取点播节目时长，如果是直播返回0，否则返回秒数
 */
API_EXPORT float API_CALL mk_player_duration(mk_player ctx);

/**
 * 获取点播播放进度，取值范围未 0.0～1.0
 */
API_EXPORT float API_CALL mk_player_progress(mk_player ctx);

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
