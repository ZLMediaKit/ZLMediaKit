/*
 * Copyright (c) 2016 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/xia-chu/ZLMediaKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
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

//音视频轨道
typedef void* mk_track;
//输出frame回调
typedef void(API_CALL *on_mk_frame_out)(void *user_data, mk_frame frame);

//track创建参数
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
 */
API_EXPORT mk_track API_CALL mk_track_create(int codec_id, codec_args *args);

/**
 * 减引用track对象
 * @param track track对象
 */
API_EXPORT void API_CALL mk_track_unref(mk_track track);

/**
 * 引用track对象
 * @param track track对象
 * @return 新的track引用对象
 */
API_EXPORT mk_track API_CALL mk_track_ref(mk_track track);

/**
 * 获取track 编码codec类型，请参考MKCodecXXX定义
 */
API_EXPORT int API_CALL mk_track_codec_id(mk_track track);

/**
 * 获取编码codec名称
 */
API_EXPORT const char* API_CALL mk_track_codec_name(mk_track track);

/**
 * 获取比特率信息
 */
API_EXPORT int API_CALL mk_track_bit_rate(mk_track track);

/**
 * 监听frame输出事件
 * @param track track对象
 * @param cb frame输出回调
 * @param user_data frame输出回调用户指针参数
 */
API_EXPORT void *API_CALL mk_track_add_delegate(mk_track track, on_mk_frame_out cb, void *user_data);

/**
 * 取消frame输出事件监听
 * @param track track对象
 * @param tag mk_track_add_delegate返回值
 */
API_EXPORT void API_CALL mk_track_del_delegate(mk_track track, void *tag);

/**
 * 输入frame到track，通常你不需要调用此api
 */
API_EXPORT void API_CALL mk_track_input_frame(mk_track track, mk_frame frame);

/**
 * track是否为视频
 */
API_EXPORT int API_CALL mk_track_is_video(mk_track track);

/**
 * 获取视频宽度
 */
API_EXPORT int API_CALL mk_track_video_width(mk_track track);

/**
 * 获取视频高度
 */
API_EXPORT int API_CALL mk_track_video_height(mk_track track);

/**
 * 获取视频帧率
 */
API_EXPORT int API_CALL mk_track_video_fps(mk_track track);

/**
 * 获取音频采样率
 */
API_EXPORT int API_CALL mk_track_audio_sample_rate(mk_track track);

/**
 * 获取音频通道数
 */
API_EXPORT int API_CALL mk_track_audio_channel(mk_track track);

/**
 * 获取音频位数，一般为16bit
 */
API_EXPORT int API_CALL mk_track_audio_sample_bit(mk_track track);

#ifdef __cplusplus
}
#endif

#endif //ZLMEDIAKIT_MK_TRACK_H