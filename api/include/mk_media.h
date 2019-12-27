/*
 * MIT License
 *
 * Copyright (c) 2019 xiongziliang <771730766@qq.com>
 *
 * This file is part of ZLMediaKit(https://github.com/xiongziliang/ZLMediaKit).
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
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
 * @param hls_enabled 是否生成hls
 * @param mp4_enabled 是否生成mp4
 * @return 对象指针
 */
API_EXPORT mk_media API_CALL mk_media_create(const char *vhost, const char *app, const char *stream, float duration, int hls_enabled, int mp4_enabled);

/**
 * 销毁媒体源
 * @param ctx 对象指针
 */
API_EXPORT void API_CALL mk_media_release(mk_media ctx);

/**
 * 添加h264视频轨道
 * @param ctx 对象指针
 * @param width 视频宽度
 * @param height 视频高度
 * @param fps 视频fps
 */
API_EXPORT void API_CALL mk_media_init_h264(mk_media ctx, int width, int height, int fps);

/**
 * 添加h265视频轨道
 * @param ctx 对象指针
 * @param width 视频宽度
 * @param height 视频高度
 * @param fps 视频fps
 */
API_EXPORT void API_CALL mk_media_init_h265(mk_media ctx, int width, int height, int fps);

/**
 * 添加aac音频轨道
 * @param ctx 对象指针
 * @param channel 通道数
 * @param sample_bit 采样位数，只支持16
 * @param sample_rate 采样率
 * @param profile aac编码profile，在不输入adts头时用于生产adts头
 */
API_EXPORT void API_CALL mk_media_init_aac(mk_media ctx, int channel, int sample_bit, int sample_rate, int profile);

/**
 * 输入单帧H264视频，帧起始字节00 00 01,00 00 00 01均可
 * @param ctx 对象指针
 * @param data 单帧H264数据
 * @param len 单帧H264数据字节数
 * @param dts 解码时间戳，单位毫秒
 * @param dts 播放时间戳，单位毫秒
 */
API_EXPORT void API_CALL mk_media_input_h264(mk_media ctx, void *data, int len, uint32_t dts, uint32_t pts);

/**
 * 输入单帧H265视频，帧起始字节00 00 01,00 00 00 01均可
 * @param ctx 对象指针
 * @param data 单帧H265数据
 * @param len 单帧H265数据字节数
 * @param dts 解码时间戳，单位毫秒
 * @param dts 播放时间戳，单位毫秒
 */
API_EXPORT void API_CALL mk_media_input_h265(mk_media ctx, void *data, int len, uint32_t dts, uint32_t pts);

/**
 * 输入单帧AAC音频
 * @param ctx 对象指针
 * @param data 单帧AAC数据
 * @param len 单帧AAC数据字节数
 * @param dts 时间戳，毫秒
 * @param with_adts_header data中是否包含7个字节的adts头
 */
API_EXPORT void API_CALL mk_media_input_aac(mk_media ctx, void *data, int len, uint32_t dts, int with_adts_header);

/**
 * 输入单帧AAC音频(单独指定adts头)
 * @param ctx 对象指针
 * @param data 不包含adts头的单帧AAC数据
 * @param len 单帧AAC数据字节数
 * @param dts 时间戳，毫秒
 * @param adts adts头
 */
API_EXPORT void API_CALL mk_media_input_aac1(mk_media ctx, void *data, int len, uint32_t dts, void *adts);

#ifdef __cplusplus
}
#endif

#endif /* MK_MEDIA_H_ */
