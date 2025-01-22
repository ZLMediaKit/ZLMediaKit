/*
 * Copyright (c) 2016-present The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/ZLMediaKit/ZLMediaKit).
 *
 * Use of this source code is governed by MIT-like license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#ifndef ZLMEDIAKIT_MK_TRACK_H
#define ZLMEDIAKIT_MK_TRACK_H

#include "mk_common.h"
#include "mk_frame.h"

#ifdef __cplusplus
extern "C" {
#endif

// 音视频轨道  [AUTO-TRANSLATED:cec3b225]
// Audio and video track
typedef struct mk_track_t *mk_track;
// 输出frame回调  [AUTO-TRANSLATED:4daee75b]
// Output frame callback
typedef void(API_CALL *on_mk_frame_out)(void *user_data, mk_frame frame);

// track创建参数  [AUTO-TRANSLATED:31a3c487]
// Track creation parameters
typedef union {
    struct {
        int width;
        int height;
        int fps;
    } video;

    struct {
        int channels;
        int sample_rate;
    } audio;
} codec_args;

/**
 * 创建track对象引用
 * @param codec_id 请参考MKCodecXXX 常量定义
 * @param args 视频或音频参数
 * @return track对象引用
 * Create a track object reference
 * @param codec_id Please refer to the MKCodecXXX constant definition
 * @param args Video or audio parameters
 * @return Track object reference
 
 * [AUTO-TRANSLATED:d53f3578]
 */
API_EXPORT mk_track API_CALL mk_track_create(int codec_id, codec_args *args);

/**
 * 减引用track对象
 * @param track track对象
 * Decrement the reference count of the track object
 * @param track Track object
 
 * [AUTO-TRANSLATED:50d6180e]
 */
API_EXPORT void API_CALL mk_track_unref(mk_track track);

/**
 * 引用track对象
 * @param track track对象
 * @return 新的track引用对象
 * Increment the reference count of the track object
 * @param track Track object
 * @return New track reference object
 
 * [AUTO-TRANSLATED:6492cbb1]
 */
API_EXPORT mk_track API_CALL mk_track_ref(mk_track track);

/**
 * 获取track 编码codec类型，请参考MKCodecXXX定义
 * Get the track encoding codec type, please refer to the MKCodecXXX definition
 
 * [AUTO-TRANSLATED:f90ed835]
 */
API_EXPORT int API_CALL mk_track_codec_id(mk_track track);

/**
 * 获取编码codec名称
 * Get the encoding codec name
 
 * [AUTO-TRANSLATED:f46d430e]
 */
API_EXPORT const char* API_CALL mk_track_codec_name(mk_track track);

/**
 * 获取比特率信息
 * Get the bitrate information
 
 * [AUTO-TRANSLATED:de8b48fe]
 */
API_EXPORT int API_CALL mk_track_bit_rate(mk_track track);

/**
 * 获取轨道是否已就绪，1: 已就绪，0：未就绪
 * Get whether the track is ready, 1: ready, 0: not ready
 
 * [AUTO-TRANSLATED:926d1a1a]
 */
API_EXPORT int API_CALL mk_track_ready(mk_track track);

/**
 * 获取累计帧数
 * Get the cumulative frame count
 
 * [AUTO-TRANSLATED:c30a45c6]
 */
API_EXPORT uint64_t API_CALL mk_track_frames(mk_track track);

/**
 * 获取时间，单位毫秒
 * Get the time, in milliseconds
 
 * [AUTO-TRANSLATED:37b0e1f9]
 */
API_EXPORT uint64_t API_CALL mk_track_duration(mk_track track);

/**
 * 监听frame输出事件
 * @param track track对象
 * @param cb frame输出回调
 * @param user_data frame输出回调用户指针参数
 * Listen for frame output events
 * @param track Track object
 * @param cb Frame output callback
 * @param user_data Frame output callback user pointer parameter
 
 * [AUTO-TRANSLATED:5cbd8347]
 */
API_EXPORT void *API_CALL mk_track_add_delegate(mk_track track, on_mk_frame_out cb, void *user_data);
API_EXPORT void *API_CALL mk_track_add_delegate2(mk_track track, on_mk_frame_out cb, void *user_data, on_user_data_free user_data_free);

/**
 * 取消frame输出事件监听
 * @param track track对象
 * @param tag mk_track_add_delegate返回值
 * Cancel the frame output event listener
 * @param track Track object
 * @param tag Return value of mk_track_add_delegate
 
 * [AUTO-TRANSLATED:83a9fd9f]
 */
API_EXPORT void API_CALL mk_track_del_delegate(mk_track track, void *tag);

/**
 * 输入frame到track，通常你不需要调用此api
 * Input frame to track, you usually don't need to call this api
 
 * [AUTO-TRANSLATED:ca3b03e8]
 */
API_EXPORT void API_CALL mk_track_input_frame(mk_track track, mk_frame frame);

/**
 * track是否为视频
 * Whether the track is video
 
 * [AUTO-TRANSLATED:22573187]
 */
API_EXPORT int API_CALL mk_track_is_video(mk_track track);

/**
 * 获取视频宽度
 * Get the video width
 
 * [AUTO-TRANSLATED:06a849c6]
 */
API_EXPORT int API_CALL mk_track_video_width(mk_track track);

/**
 * 获取视频高度
 * Get the video height
 
 * [AUTO-TRANSLATED:27b5ed6e]
 */
API_EXPORT int API_CALL mk_track_video_height(mk_track track);

/**
 * 获取视频帧率
 * Get the video frame rate
 
 * [AUTO-TRANSLATED:3c19a388]
 */
API_EXPORT int API_CALL mk_track_video_fps(mk_track track);

/**
 * 获取视频累计关键帧数
 * Get the cumulative number of video keyframes
 
 * [AUTO-TRANSLATED:0e70e666]
 */
API_EXPORT uint64_t API_CALL mk_track_video_key_frames(mk_track track);

/**
 * 获取视频GOP关键帧间隔
 * Get the video GOP keyframe interval
 
 * [AUTO-TRANSLATED:ea8d3729]
 */
API_EXPORT int API_CALL mk_track_video_gop_size(mk_track track);

/**
 * 获取视频累计关键帧间隔(毫秒)
 * Get the cumulative video keyframe interval (milliseconds)
 
 * [AUTO-TRANSLATED:194b1e80]
 */
API_EXPORT int API_CALL mk_track_video_gop_interval_ms(mk_track track);

/**
 * 获取音频采样率
 * Get the audio sample rate
 
 * [AUTO-TRANSLATED:bf0e046b]
 */
API_EXPORT int API_CALL mk_track_audio_sample_rate(mk_track track);

/**
 * 获取音频通道数
 * Get the number of audio channels
 
 * [AUTO-TRANSLATED:ccb5d776]
 */
API_EXPORT int API_CALL mk_track_audio_channel(mk_track track);

/**
 * 获取音频位数，一般为16bit
 * Get the audio bit depth, usually 16bit
 
 * [AUTO-TRANSLATED:11e36409]
 */
API_EXPORT int API_CALL mk_track_audio_sample_bit(mk_track track);

#ifdef __cplusplus
}
#endif

#endif //ZLMEDIAKIT_MK_TRACK_H