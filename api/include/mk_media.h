/*
 * Copyright (c) 2016 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/xia-chu/ZLMediaKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MK_MEDIA_H_
#define MK_MEDIA_H_

#include "mk_common.h"
#include "mk_track.h"
#include "mk_frame.h"
#include "mk_events_objects.h"

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
API_EXPORT mk_media API_CALL mk_media_create(const char *vhost, const char *app, const char *stream,
                                             float duration, int hls_enabled, int mp4_enabled);

/**
 * 销毁媒体源
 * @param ctx 对象指针
 */
API_EXPORT void API_CALL mk_media_release(mk_media ctx);

/**
 * 添加音视频track
 * @param ctx mk_media对象
 * @param track mk_track对象，音视频轨道
 */
API_EXPORT void API_CALL mk_media_init_track(mk_media ctx, mk_track track);

/**
 * 添加视频轨道，请改用mk_media_init_track方法
 * @param ctx 对象指针
 * @param codec_id  0:CodecH264/1:CodecH265
 * @param width 视频宽度; 在编码时才有效
 * @param height 视频高度; 在编码时才有效
 * @param fps 视频fps; 在编码时才有效
 * @param bit_rate 视频比特率,单位bps; 在编码时才有效
 * @param width 视频宽度
 * @param height 视频高度
 * @param fps 视频fps
 * @return 1代表成功，0失败
 */
API_EXPORT int API_CALL mk_media_init_video(mk_media ctx, int codec_id, int width, int height, float fps, int bit_rate);

/**
 * 添加音频轨道，请改用mk_media_init_track方法
 * @param ctx 对象指针
 * @param codec_id  2:CodecAAC/3:CodecG711A/4:CodecG711U/5:OPUS
 * @param channel 通道数
 * @param sample_bit 采样位数，只支持16
 * @param sample_rate 采样率
 * @return 1代表成功，0失败
 */
API_EXPORT int API_CALL mk_media_init_audio(mk_media ctx, int codec_id, int sample_rate, int channels, int sample_bit);

/**
 * 初始化h264/h265/aac完毕后调用此函数，
 * 在单track(只有音频或视频)时，因为ZLMediaKit不知道后续是否还要添加track，所以会多等待3秒钟
 * 如果产生的流是单Track类型，请调用此函数以便加快流生成速度，当然不调用该函数，影响也不大(会多等待3秒)
 * @param ctx 对象指针
 */
API_EXPORT void API_CALL mk_media_init_complete(mk_media ctx);

/**
 * 输入frame对象
 * @param ctx mk_media对象
 * @param frame 帧对象
 * @return 1代表成功，0失败
 */
API_EXPORT int API_CALL mk_media_input_frame(mk_media ctx, mk_frame frame);

/**
 * 输入单帧H264视频，帧起始字节00 00 01,00 00 00 01均可，请改用mk_media_input_frame方法
 * @param ctx 对象指针
 * @param data 单帧H264数据
 * @param len 单帧H264数据字节数
 * @param dts 解码时间戳，单位毫秒
 * @param pts 播放时间戳，单位毫秒
 * @return 1代表成功，0失败
 */
API_EXPORT int API_CALL mk_media_input_h264(mk_media ctx, const void *data, int len, uint64_t dts, uint64_t pts);

/**
 * 输入单帧H265视频，帧起始字节00 00 01,00 00 00 01均可，请改用mk_media_input_frame方法
 * @param ctx 对象指针
 * @param data 单帧H265数据
 * @param len 单帧H265数据字节数
 * @param dts 解码时间戳，单位毫秒
 * @param pts 播放时间戳，单位毫秒
 * @return 1代表成功，0失败
 */
API_EXPORT int API_CALL mk_media_input_h265(mk_media ctx, const void *data, int len, uint64_t dts, uint64_t pts);

/**
 * 输入YUV视频数据
 * @param ctx 对象指针
 * @param yuv yuv420p数据
 * @param linesize yuv420p linesize
 * @param cts 视频采集时间戳，单位毫秒
 */
API_EXPORT void API_CALL mk_media_input_yuv(mk_media ctx, const char *yuv[3], int linesize[3], uint64_t cts);

/**
 * 输入单帧AAC音频(单独指定adts头)，请改用mk_media_input_frame方法
 * @param ctx 对象指针
 * @param data 不包含adts头的单帧AAC数据，adts头7个字节
 * @param len 单帧AAC数据字节数
 * @param dts 时间戳，毫秒
 * @param adts adts头，可以为null
 * @return 1代表成功，0失败
 */
API_EXPORT int API_CALL mk_media_input_aac(mk_media ctx, const void *data, int len, uint64_t dts, void *adts);

/**
 * 输入单帧PCM音频,启用ENABLE_FAAC编译时，该函数才有效
 * @param ctx 对象指针
 * @param data 单帧PCM数据
 * @param len 单帧PCM数据字节数
 * @param dts 时间戳，毫秒
 * @return 1代表成功，0失败
 */
API_EXPORT int API_CALL mk_media_input_pcm(mk_media ctx, void *data, int len, uint64_t pts);

/**
 * 输入单帧OPUS/G711音频帧，请改用mk_media_input_frame方法
 * @param ctx 对象指针
 * @param data 单帧音频数据
 * @param len  单帧音频数据字节数
 * @param dts 时间戳，毫秒
 * @return 1代表成功，0失败
 */
API_EXPORT int API_CALL mk_media_input_audio(mk_media ctx, const void* data, int len, uint64_t dts);

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
 * 收到客户端的pause或resume请求时触发该回调
 * @param user_data 用户数据指针,通过mk_media_set_on_pause设置
 * @param pause 1:暂停, 0: 恢复
 */
typedef int(API_CALL* on_mk_media_pause)(void* user_data, int pause);

/**
 * 收到客户端的speed请求时触发该回调
 * @param user_data 用户数据指针,通过mk_media_set_on_pause设置
 * @param speed 0.5 1.0 2.0
 */
typedef int(API_CALL* on_mk_media_speed)(void* user_data, float speed);

/**
 * 监听播放器seek请求事件
 * @param ctx 对象指针
 * @param cb 回调指针
 * @param user_data 用户数据指针
 */
API_EXPORT void API_CALL mk_media_set_on_seek(mk_media ctx, on_mk_media_seek cb, void *user_data);

/**
 * 监听播放器pause请求事件
 * @param ctx 对象指针
 * @param cb 回调指针
 * @param user_data 用户数据指针
 */
API_EXPORT void API_CALL mk_media_set_on_pause(mk_media ctx, on_mk_media_pause cb, void* user_data);

/**
 * 监听播放器pause请求事件
 * @param ctx 对象指针
 * @param cb 回调指针
 * @param user_data 用户数据指针
 */
API_EXPORT void API_CALL mk_media_set_on_speed(mk_media ctx, on_mk_media_speed cb, void* user_data);

/**
 * 获取总的观看人数
 * @param ctx 对象指针
 * @return 观看人数
 */
API_EXPORT int API_CALL mk_media_total_reader_count(mk_media ctx);

/**
 * 生成的MediaSource注册或注销事件
 * @param user_data 设置回调时的用户数据指针
 * @param sender 生成的MediaSource对象
 * @param regist 1为注册事件，0为注销事件
 */
typedef void(API_CALL *on_mk_media_source_regist)(void *user_data, mk_media_source sender, int regist);

/**
 * 设置MediaSource注册或注销事件回调函数
 * @param ctx 对象指针
 * @param cb 回调指针
 * @param user_data 用户数据指针
 */
API_EXPORT void API_CALL mk_media_set_on_regist(mk_media ctx, on_mk_media_source_regist cb, void *user_data);

/**
 * rtp推流成功与否的回调(第一次成功后，后面将一直重试)
 */
typedef on_mk_media_source_send_rtp_result on_mk_media_send_rtp_result;

/**
 * 开始发送一路ps-rtp流(通过ssrc区分多路)
 * @param ctx 对象指针
 * @param dst_url 目标ip或域名
 * @param dst_port 目标端口
 * @param ssrc rtp的ssrc，10进制的字符串打印
 * @param is_udp 是否为udp
 * @param cb 启动成功或失败回调
 * @param user_data 回调用户指针
 */
API_EXPORT void API_CALL mk_media_start_send_rtp(mk_media ctx, const char *dst_url, uint16_t dst_port, const char *ssrc, int is_udp, on_mk_media_send_rtp_result cb, void *user_data);

/**
 * 停止某路或全部ps-rtp发送
 * @param ctx 对象指针
 * @param ssrc rtp的ssrc，10进制的字符串打印，如果为null或空字符串，则停止所有rtp推流
 * @return 1成功，0失败
 */
API_EXPORT int API_CALL mk_media_stop_send_rtp(mk_media ctx, const char *ssrc);

#ifdef __cplusplus
}
#endif

#endif /* MK_MEDIA_H_ */
