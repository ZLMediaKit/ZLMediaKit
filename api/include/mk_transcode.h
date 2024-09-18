/*
 * Copyright (c) 2016-present The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/ZLMediaKit/ZLMediaKit).
 *
 * Use of this source code is governed by MIT-like license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#ifndef ZLMEDIAKIT_MK_TRANSCODE_H
#define ZLMEDIAKIT_MK_TRANSCODE_H

#include "mk_common.h"
#include "mk_track.h"
#include "mk_tcp.h"

#ifdef __cplusplus
extern "C" {
#endif

// 解码器对象  [AUTO-TRANSLATED:14f75955]
// cpp
// Decoder object
typedef struct mk_decoder_t *mk_decoder;
// 解码后的frame  [AUTO-TRANSLATED:acb96f85]
// Decoded frame
typedef struct mk_frame_pix_t *mk_frame_pix;
// SwsContext的包装  [AUTO-TRANSLATED:4f7ae38f]
// SwsContext wrapper
typedef struct mk_swscale_t *mk_swscale;
// FFmpeg原始解码帧对象  [AUTO-TRANSLATED:ed99009b]
// FFmpeg original decoded frame object
typedef struct AVFrame AVFrame;
// FFmpeg编解码器对象  [AUTO-TRANSLATED:12b26186]
// FFmpeg codec object
typedef struct AVCodecContext AVCodecContext;
// 解码输出回调  [AUTO-TRANSLATED:1a380eed]
// Decode output callback
typedef void(API_CALL *on_mk_decode)(void *user_data, mk_frame_pix frame);

/**
 * 创建解码器
 * @param track track对象
 * @param thread_num 解码线程数，0时为自动
 * @return 返回解码器对象，NULL代表失败
 * Create decoder
 * @param track track object
 * @param thread_num Number of decoding threads, 0 for automatic
 * @return Returns the decoder object, NULL indicates failure
 
 * [AUTO-TRANSLATED:d01b3192]
 */
API_EXPORT mk_decoder API_CALL mk_decoder_create(mk_track track, int thread_num);

/**
 * 创建解码器
 * @param track track对象
 * @param thread_num 解码线程数，0时为自动
 * @param codec_name_list 偏好的ffmpeg codec name列表，以NULL结尾，譬如：{"libopenh264", "h264_nvdec", NULL};
 *                        在数组中越前，优先级越高;如果指定的codec不存在，或跟mk_track_codec_id类型不匹配时，则使用内部默认codec列表
 * @return 返回解码器对象，NULL代表失败
 * Create decoder
 * @param track track object
 * @param thread_num Number of decoding threads, 0 for automatic
 * @param codec_name_list Preferred ffmpeg codec name list, ending with NULL, for example: {"libopenh264", "h264_nvdec", NULL};
 *                        The higher the priority in the array, the higher the priority; if the specified codec does not exist, or does not match the mk_track_codec_id type, the internal default codec list will be used
 * @return Returns the decoder object, NULL indicates failure
 
 * [AUTO-TRANSLATED:078aba31]
 */
API_EXPORT mk_decoder API_CALL mk_decoder_create2(mk_track track, int thread_num, const char *codec_name_list[]);

/**
 * 销毁解码器
 * @param ctx 解码器对象
 * @param flush_frame 是否等待所有帧解码成功
 * Destroy decoder
 * @param ctx Decoder object
 * @param flush_frame Whether to wait for all frames to be decoded successfully
 
 * [AUTO-TRANSLATED:1a4d9663]
 */
API_EXPORT void API_CALL mk_decoder_release(mk_decoder ctx,  int flush_frame);

/**
 * 解码音视频帧
 * @param ctx 解码器
 * @param frame 帧对象
 * @param async 是否异步解码
 * @param enable_merge 是否合并帧解码，有些情况下，需要把时间戳相同的slice合并输入到解码器才能解码
 * Decode audio and video frames
 * @param ctx Decoder
 * @param frame Frame object
 * @param async Whether to decode asynchronously
 * @param enable_merge Whether to merge frame decoding, in some cases, it is necessary to merge slices with the same timestamp into the decoder before decoding
 
 * [AUTO-TRANSLATED:87df4c4d]
 */
API_EXPORT void API_CALL mk_decoder_decode(mk_decoder ctx, mk_frame frame, int async, int enable_merge);

/**
 * 设置异步解码最大帧缓存积压数限制
 * Set the maximum frame cache backlog limit for asynchronous decoding
 
 * [AUTO-TRANSLATED:1e3e413d]
 */
API_EXPORT void API_CALL mk_decoder_set_max_async_frame_size(mk_decoder ctx, size_t size);

/**
 * 设置解码输出回调
 * @param ctx 解码器
 * @param cb 回调函数
 * @param user_data 回调函数用户指针参数
 * Set decode output callback
 * @param ctx Decoder
 * @param cb Callback function
 * @param user_data User pointer parameter of the callback function
 
 * [AUTO-TRANSLATED:a90f8764]
 */
API_EXPORT void API_CALL mk_decoder_set_cb(mk_decoder ctx, on_mk_decode cb, void *user_data);
API_EXPORT void API_CALL mk_decoder_set_cb2(mk_decoder ctx, on_mk_decode cb, void *user_data, on_user_data_free user_data_free);

/**
 * 获取FFmpeg原始AVCodecContext对象
 * @param ctx 解码器
 * Get the FFmpeg original AVCodecContext object
 * @param ctx Decoder
 
 * [AUTO-TRANSLATED:73ed5496]
 */
API_EXPORT const AVCodecContext* API_CALL mk_decoder_get_context(mk_decoder ctx);

/////////////////////////////////////////////////////////////////////////////////////////////

/**
 * 创建解码帧mk_frame_pix新引用
 * @param frame 原始引用
 * @return 新引用
 * Create a new reference to the mk_frame_pix decoding frame
 * @param frame Original reference
 * @return New reference
 
 * [AUTO-TRANSLATED:ca58ab5d]
 */
API_EXPORT mk_frame_pix API_CALL mk_frame_pix_ref(mk_frame_pix frame);

/**
 * 解码帧mk_frame_pix减引用
 * @param frame 原始引用
 * Decrease the reference of the decoding frame mk_frame_pix
 * @param frame Original reference
 
 * [AUTO-TRANSLATED:1581d0a9]
 */
API_EXPORT void API_CALL mk_frame_pix_unref(mk_frame_pix frame);

/**
 * 从FFmpeg AVFrame转换为mk_frame_pix
 * @param frame FFmpeg AVFrame
 * @return mk_frame_pix对象
 * Convert from FFmpeg AVFrame to mk_frame_pix
 * @param frame FFmpeg AVFrame
 * @return mk_frame_pix object
 
 * [AUTO-TRANSLATED:adfb43d5]
 */
API_EXPORT mk_frame_pix API_CALL mk_frame_pix_from_av_frame(AVFrame *frame);

/**
 * 可无内存拷贝的创建mk_frame_pix对象
 * @param plane_data 多个平面数据, 通过mk_buffer_get_data获取其数据指针
 * @param line_size 平面数据line size
 * @param plane 数据平面个数
 * @return mk_frame_pix对象
 * Create a mk_frame_pix object without memory copy
 * @param plane_data Multiple plane data, get its data pointer through mk_buffer_get_data
 * @param line_size Plane data line size
 * @param plane Number of data planes
 * @return mk_frame_pix object
 
 * [AUTO-TRANSLATED:b720d2e2]
 */
API_EXPORT mk_frame_pix API_CALL mk_frame_pix_from_buffer(mk_buffer plane_data[], int line_size[], int plane);

/**
 * 获取FFmpeg AVFrame对象
 * @param frame 解码帧mk_frame_pix
 * @return FFmpeg AVFrame对象
 * Get the FFmpeg AVFrame object
 * @param frame Decoded frame mk_frame_pix
 * @return FFmpeg AVFrame object
 
 * [AUTO-TRANSLATED:03142bdc]
 */
API_EXPORT AVFrame* API_CALL mk_frame_pix_get_av_frame(mk_frame_pix frame);

/////////////////////////////////////////////////////////////////////////////////////////////

/**
 * 创建ffmpeg SwsContext wrapper实例
 * @param output AVPixelFormat类型，AV_PIX_FMT_BGR24==3
 * @param width 目标宽度，置0时，则与输入时一致
 * @param height 目标高度，置0时，则与输入时一致
 * @return SwsContext wrapper 实例
 * Create an instance of the ffmpeg SwsContext wrapper
 * @param output AVPixelFormat type, AV_PIX_FMT_BGR24==3
 * @param width Target width, set to 0, then it is the same as the input
 * @param height Target height, set to 0, then it is the same as the input
 * @return SwsContext wrapper instance
 
 * [AUTO-TRANSLATED:417474cb]
 */
API_EXPORT mk_swscale mk_swscale_create(int output, int width, int height);

/**
 * 释放ffmpeg SwsContext wrapper实例
 * @param ctx SwsContext wrapper实例
 * Release the ffmpeg SwsContext wrapper instance
 * @param ctx SwsContext wrapper instance
 
 * [AUTO-TRANSLATED:8eaaea2f]
 */
API_EXPORT void mk_swscale_release(mk_swscale ctx);

/**
 * 使用SwsContext转换pix format
 * @param ctx SwsContext wrapper实例
 * @param frame pix frame
 * @param out 转换后存放的数据指针，用户需要确保提前申请并大小足够
 * @return sws_scale()返回值：the height of the output slice
 * Use SwsContext to convert pix format
 * @param ctx SwsContext wrapper instance
 * @param frame pix frame
 * @param out Data pointer to store the converted data, the user needs to ensure that the application is applied in advance and the size is sufficient
 * @return sws_scale() return value: the height of the output slice
 
 * [AUTO-TRANSLATED:3018afe4]
 */
API_EXPORT int mk_swscale_input_frame(mk_swscale ctx, mk_frame_pix frame, uint8_t *out);

/**
 *  使用SwsContext转换pix format
 * @param ctx SwsContext wrapper实例
 * @param frame pix frame
 * @return 新的pix frame对象，需要使用mk_frame_pix_unref销毁
 * Use SwsContext to convert pix format
 * @param ctx SwsContext wrapper instance
 * @param frame pix frame
 * @return New pix frame object, needs to be destroyed using mk_frame_pix_unref
 
 
 * [AUTO-TRANSLATED:5b4e98a3]
 */
API_EXPORT mk_frame_pix mk_swscale_input_frame2(mk_swscale ctx, mk_frame_pix frame);

/////////////////////////////////////////////////////////////////////////////////////////////

API_EXPORT uint8_t **API_CALL mk_get_av_frame_data(AVFrame *frame);
API_EXPORT void API_CALL mk_set_av_frame_data(AVFrame *frame, uint8_t *data, int plane);

API_EXPORT int *API_CALL mk_get_av_frame_line_size(AVFrame *frame);
API_EXPORT void API_CALL mk_set_av_frame_line_size(AVFrame *frame, int line_size, int plane);

API_EXPORT int64_t API_CALL mk_get_av_frame_dts(AVFrame *frame);
API_EXPORT void API_CALL mk_set_av_frame_dts(AVFrame *frame, int64_t dts);

API_EXPORT int64_t API_CALL mk_get_av_frame_pts(AVFrame *frame);
API_EXPORT void API_CALL mk_set_av_frame_pts(AVFrame *frame, int64_t pts);

API_EXPORT int API_CALL mk_get_av_frame_width(AVFrame *frame);
API_EXPORT void API_CALL mk_set_av_frame_width(AVFrame *frame, int width);

API_EXPORT int API_CALL mk_get_av_frame_height(AVFrame *frame);
API_EXPORT void API_CALL mk_set_av_frame_height(AVFrame *frame, int height);

API_EXPORT int API_CALL mk_get_av_frame_format(AVFrame *frame);
API_EXPORT void API_CALL mk_set_av_frame_format(AVFrame *frame, int format);

#ifdef __cplusplus
}
#endif

#endif //ZLMEDIAKIT_MK_TRANSCODE_H