/*
 * Copyright (c) 2016 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/xiongziliang/ZLMediaKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MK_MEDIA_H_
#define MK_MEDIA_H_

#include "mk_common.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef void *mk_media;

/**
 * 创建一个媒体源
 * @param vhost 虚拟主机名，一般为__defaultVhost__
 * @param app 应用名，推荐为live
 * @param stream 流id，例如camera
 * @param duration 时长(单位秒)，直播则为0
 * @param rtsp_enabled 是否启用rtsp协议
 * @param rtmp_enabled 是否启用rtmp协议
 * @param hls_enabled 是否生成hls
 * @param mp4_enabled 是否生成mp4
 * @return 对象指针
 */
API_EXPORT mk_media API_CALL mk_media_create(const char *vhost, const char *app, const char *stream, float duration,
                                             int rtsp_enabled, int rtmp_enabled, int hls_enabled, int mp4_enabled);

/**
 * 销毁媒体源
 * @param ctx 对象指针
 */
API_EXPORT void API_CALL mk_media_release(mk_media ctx);

/**
 * 添加视频轨道
 * @param ctx 对象指针
 * @param track_id  0:CodecH264/1:CodecH265
 * @param width 视频宽度
 * @param height 视频高度
 * @param fps 视频fps
 */
API_EXPORT void API_CALL mk_media_init_video(mk_media ctx, int track_id, int width, int height, int fps);

/**
 * 添加音频轨道
 * @param ctx 对象指针
 * @param track_id  2:CodecAAC/3:CodecG711A/4:CodecG711U
 * @param channel 通道数
 * @param sample_bit 采样位数，只支持16
 * @param sample_rate 采样率
 */
API_EXPORT void API_CALL mk_media_init_audio(mk_media ctx, int track_id, int sample_rate, int channels, int sample_bit);

/**
 * 初始化h264/h265/aac完毕后调用此函数，
 * 在单track(只有音频或视频)时，因为ZLMediaKit不知道后续是否还要添加track，所以会多等待3秒钟
 * 如果产生的流是单Track类型，请调用此函数以便加快流生成速度，当然不调用该函数，影响也不大(会多等待3秒)
 * @param ctx 对象指针
 */
API_EXPORT void API_CALL mk_media_init_complete(mk_media ctx);

/**
 * 输入单帧H264视频，帧起始字节00 00 01,00 00 00 01均可
 * @param ctx 对象指针
 * @param data 单帧H264数据
 * @param len 单帧H264数据字节数
 * @param dts 解码时间戳，单位毫秒
 * @param pts 播放时间戳，单位毫秒
 */
API_EXPORT void API_CALL mk_media_input_h264(mk_media ctx, void *data, int len, uint32_t dts, uint32_t pts);

/**
 * 输入单帧H265视频，帧起始字节00 00 01,00 00 00 01均可
 * @param ctx 对象指针
 * @param data 单帧H265数据
 * @param len 单帧H265数据字节数
 * @param dts 解码时间戳，单位毫秒
 * @param pts 播放时间戳，单位毫秒
 */
API_EXPORT void API_CALL mk_media_input_h265(mk_media ctx, void *data, int len, uint32_t dts, uint32_t pts);

/**
 * 输入单帧AAC音频(单独指定adts头)
 * @param ctx 对象指针
 * @param data 不包含adts头的单帧AAC数据
 * @param len 单帧AAC数据字节数
 * @param dts 时间戳，毫秒
 * @param adts adts头
 */
API_EXPORT void API_CALL mk_media_input_aac(mk_media ctx, void *data, int len, uint32_t dts, void *adts);

/**
 * 输入单帧G711音频
 * @param ctx 对象指针
 * @param data 单帧G711数据
 * @param len 单帧G711数据字节数
 * @param dts 时间戳，毫秒
 */
API_EXPORT void API_CALL mk_media_input_g711(mk_media ctx, void* data, int len, uint32_t dts);

/**
 * MediaSource.close()回调事件
 * 在选择关闭一个关联的MediaSource时，将会最终触发到该回调
 * 你应该通过该事件调用mk_media_release函数并且释放其他资源
 * 如果你不调用mk_media_release函数，那么MediaSource.close()操作将无效
 * @param user_data 用户数据指针，通过mk_media_set_on_close函数设置
 */
typedef void(API_CALL *on_mk_media_close)(void *user_data);

/**
 * 监听MediaSource.close()事件
 * 在选择关闭一个关联的MediaSource时，将会最终触发到该回调
 * 你应该通过该事件调用mk_media_release函数并且释放其他资源
 * @param ctx 对象指针
 * @param cb 回调指针
 * @param user_data 用户数据指针
 */
API_EXPORT void API_CALL mk_media_set_on_close(mk_media ctx, on_mk_media_close cb, void *user_data);

/**
 * 收到客户端的seek请求时触发该回调
 * @param user_data 用户数据指针,通过mk_media_set_on_seek设置
 * @param stamp_ms seek至的时间轴位置，单位毫秒
 * @return 1代表将处理seek请求，0代表忽略该请求
 */
typedef int(API_CALL *on_mk_media_seek)(void *user_data,uint32_t stamp_ms);

/**
 * 监听播放器seek请求事件
 * @param ctx 对象指针
 * @param cb 回调指针
 * @param user_data 用户数据指针
 */
API_EXPORT void API_CALL mk_media_set_on_seek(mk_media ctx, on_mk_media_seek cb, void *user_data);

/**
 * 获取总的观看人数
 * @param ctx 对象指针
 * @return 观看人数
 */
API_EXPORT int API_CALL mk_media_total_reader_count(mk_media ctx);

#ifdef __cplusplus
}
#endif

#endif /* MK_MEDIA_H_ */
