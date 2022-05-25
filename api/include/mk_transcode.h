/*
 * Copyright (c) 2016 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/xia-chu/ZLMediaKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#ifndef ZLMEDIAKIT_MK_TRANSCODE_H
#define ZLMEDIAKIT_MK_TRANSCODE_H

#include "mk_common.h"
#include "mk_track.h"

#ifdef __cplusplus
extern "C" {
#endif

//解码器对象
typedef void *mk_decoder;
//解码后的frame
typedef void *mk_frame_pix;
//SwsContext的包装
typedef void *mk_swscale;
//FFmpeg原始解码帧对象
typedef struct AVFrame AVFrame;
//FFmpeg编解码器对象
typedef struct AVCodecContext AVCodecContext;
//解码输出回调
typedef void(API_CALL *on_mk_decode)(void *user_data, mk_frame_pix frame);

/**
 * 创建解码器
 * @param track track对象
 * @param thread_num 解码线程数，0时为自动
 * @return 返回解码器对象，NULL代表失败
 */
API_EXPORT mk_decoder API_CALL mk_decoder_create(mk_track track, int thread_num);

/**
 * 销毁解码器
 * @param ctx 解码器对象
 * @param flush_frame 是否等待所有帧解码成功
 */
API_EXPORT void API_CALL mk_decoder_release(mk_decoder ctx,  int flush_frame);

/**
 * 解码音视频帧
 * @param ctx 解码器
 * @param frame 帧对象
 * @param async 是否异步解码
 * @param enable_merge 是否合并帧解码，有些情况下，需要把时间戳相同的slice合并输入到解码器才能解码
 */
API_EXPORT void API_CALL mk_decoder_decode(mk_decoder ctx, mk_frame frame, int async, int enable_merge);

/**
 * 设置异步解码最大帧缓存积压数限制
 */
API_EXPORT void API_CALL mk_decoder_set_max_async_frame_size(mk_decoder ctx, size_t size);

/**
 * 设置解码输出回调
 * @param ctx 解码器
 * @param cb 回调函数
 * @param user_data 回调函数用户指针参数
 */
API_EXPORT void API_CALL mk_decoder_set_cb(mk_decoder ctx, on_mk_decode cb, void *user_data);

/**
 * 获取FFmpeg原始AVCodecContext对象
 * @param ctx 解码器
 */
API_EXPORT const AVCodecContext* API_CALL mk_decoder_get_context(mk_decoder ctx);

/////////////////////////////////////////////////////////////////////////////////////////////

/**
 * 创建解码帧mk_frame_pix新引用
 * @param frame 原始引用
 * @return 新引用
 */
API_EXPORT mk_frame_pix API_CALL mk_frame_pix_ref(mk_frame_pix frame);

/**
 * 解码帧mk_frame_pix减引用
 * @param frame 原始引用
 */
API_EXPORT void API_CALL mk_frame_pix_unref(mk_frame_pix frame);

/**
 * 从FFmpeg AVFrame转换为mk_frame_pix
 * @param frame FFmpeg AVFrame
 * @return mk_frame_pix对象
 */
API_EXPORT mk_frame_pix API_CALL mk_frame_pix_from_av_frame(AVFrame *frame);

/**
 * 获取FFmpeg AVFrame对象
 * @param frame 解码帧mk_frame_pix
 * @return FFmpeg AVFrame对象
 */
API_EXPORT AVFrame* API_CALL mk_frame_pix_get_av_frame(mk_frame_pix frame);

/////////////////////////////////////////////////////////////////////////////////////////////

/**
 * 创建ffmpeg SwsContext wrapper实例
 * @param output AVPixelFormat类型，AV_PIX_FMT_BGR24==3
 * @param width 目标宽度，置0时，则与输入时一致
 * @param height 目标高度，置0时，则与输入时一致
 * @return SwsContext wrapper 实例
 */
API_EXPORT mk_swscale mk_swscale_create(int output, int width, int height);

/**
 * 释放ffmpeg SwsContext wrapper实例
 * @param ctx SwsContext wrapper实例
 */
API_EXPORT void mk_swscale_release(mk_swscale ctx);

/**
 * 使用SwsContext转换pix format
 * @param ctx SwsContext wrapper实例
 * @param frame pix frame
 * @param out 转换后存放的数据指针，用户需要确保提前申请并大小足够
 * @return sws_scale()返回值：the height of the output slice
 */
API_EXPORT int mk_swscale_input_frame(mk_swscale ctx, mk_frame_pix frame, uint8_t *out);

/**
 *  使用SwsContext转换pix format
 * @param ctx SwsContext wrapper实例
 * @param frame pix frame
 * @return 新的pix frame对象，需要使用mk_frame_pix_unref销毁
 */
API_EXPORT mk_frame_pix mk_swscale_input_frame2(mk_swscale ctx, mk_frame_pix frame);

/////////////////////////////////////////////////////////////////////////////////////////////

API_EXPORT uint8_t** API_CALL mk_get_av_frame_data(AVFrame *frame);
API_EXPORT int* API_CALL mk_get_av_frame_line_size(AVFrame *frame);
API_EXPORT int64_t API_CALL mk_get_av_frame_dts(AVFrame *frame);
API_EXPORT int64_t API_CALL mk_get_av_frame_pts(AVFrame *frame);
API_EXPORT int API_CALL mk_get_av_frame_width(AVFrame *frame);
API_EXPORT int API_CALL mk_get_av_frame_height(AVFrame *frame);
API_EXPORT int API_CALL mk_get_av_frame_format(AVFrame *frame);

#ifdef __cplusplus
}
#endif

#endif //ZLMEDIAKIT_MK_TRANSCODE_H