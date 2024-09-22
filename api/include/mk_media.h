/*
 * Copyright (c) 2016-present The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/ZLMediaKit/ZLMediaKit).
 *
 * Use of this source code is governed by MIT-like license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MK_MEDIA_H_
#define MK_MEDIA_H_

#include "mk_common.h"
#include "mk_track.h"
#include "mk_frame.h"
#include "mk_events_objects.h"
#include "mk_thread.h"
#include "mk_util.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct mk_media_t *mk_media;

/**
 * 创建一个媒体源
 * @param vhost 虚拟主机名，一般为__defaultVhost__
 * @param app 应用名，推荐为live
 * @param stream 流id，例如camera
 * @param duration 时长(单位秒)，直播则为0
 * @param hls_enabled 是否生成hls
 * @param mp4_enabled 是否生成mp4
 * @return 对象指针
 * Create a media source
 * @param vhost Virtual host name, generally __defaultVhost__
 * @param app Application name, recommended as live
 * @param stream Stream id, such as camera
 * @param duration Duration (in seconds), 0 for live broadcast
 * @param hls_enabled Whether to generate hls
 * @param mp4_enabled Whether to generate mp4
 * @return Object pointer
 
 * [AUTO-TRANSLATED:b5124a1e]
 */
API_EXPORT mk_media API_CALL mk_media_create(const char *vhost, const char *app, const char *stream,
                                             float duration, int hls_enabled, int mp4_enabled);

/**
 * 创建一个媒体源
 * @param vhost 虚拟主机名，一般为__defaultVhost__
 * @param app 应用名，推荐为live
 * @param stream 流id，例如camera
 * @param duration 时长(单位秒)，直播则为0
 * @param option ProtocolOption相关配置
 * @return 对象指针
 * Create a media source
 * @param vhost Virtual host name, generally __defaultVhost__
 * @param app Application name, recommended as live
 * @param stream Stream id, such as camera
 * @param duration Duration (in seconds), 0 for live broadcast
 * @param option ProtocolOption related configuration
 * @return Object pointer
 
 * [AUTO-TRANSLATED:870d86b0]
 */
API_EXPORT mk_media API_CALL mk_media_create2(const char *vhost, const char *app, const char *stream, float duration, mk_ini option);

/**
 * 销毁媒体源
 * @param ctx 对象指针
 * Destroy the media source
 * @param ctx Object pointer
 
 * [AUTO-TRANSLATED:a63ad166]
 */
API_EXPORT void API_CALL mk_media_release(mk_media ctx);

/**
 * 添加音视频track
 * @param ctx mk_media对象
 * @param track mk_track对象，音视频轨道
 * Add audio and video tracks
 * @param ctx mk_media object
 * @param track mk_track object, audio and video track
 
 * [AUTO-TRANSLATED:0e4ebe8d]
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
 * Add video track, please use mk_media_init_track method
 * @param ctx Object pointer
 * @param codec_id  0:CodecH264/1:CodecH265
 * @param width Video width; Valid only during encoding
 * @param height Video height; Valid only during encoding
 * @param fps Video fps; Valid only during encoding
 * @param bit_rate Video bitrate, unit bps; Valid only during encoding
 * @param width Video width
 * @param height Video height
 * @param fps Video fps
 * @return 1 for success, 0 for failure
 
 * [AUTO-TRANSLATED:c6944851]
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
 * Add audio track, please use mk_media_init_track method
 * @param ctx Object pointer
 * @param codec_id  2:CodecAAC/3:CodecG711A/4:CodecG711U/5:OPUS
 * @param channel Number of channels
 * @param sample_bit Sampling bit, only supports 16
 * @param sample_rate Sampling rate
 * @return 1 for success, 0 for failure
 
 * [AUTO-TRANSLATED:5c5c7c7a]
 */
API_EXPORT int API_CALL mk_media_init_audio(mk_media ctx, int codec_id, int sample_rate, int channels, int sample_bit);

/**
 * 初始化h264/h265/aac完毕后调用此函数，
 * 在单track(只有音频或视频)时，因为ZLMediaKit不知道后续是否还要添加track，所以会多等待3秒钟
 * 如果产生的流是单Track类型，请调用此函数以便加快流生成速度，当然不调用该函数，影响也不大(会多等待3秒)
 * @param ctx 对象指针
 * Call this function after h264/h265/aac initialization,
 * In single track (only audio or video), because ZLMediaKit does not know whether to add more tracks later, it will wait for 3 seconds.
 * If the generated stream is a single Track type, please call this function to speed up the stream generation speed. Of course, if you do not call this function, the impact is not big (it will wait for 3 seconds).
 * @param ctx Object pointer
 
 * [AUTO-TRANSLATED:cd2bee12]
 */
API_EXPORT void API_CALL mk_media_init_complete(mk_media ctx);

/**
 * 输入frame对象
 * @param ctx mk_media对象
 * @param frame 帧对象
 * @return 1代表成功，0失败
 * Input frame object
 * @param ctx mk_media object
 * @param frame Frame object
 * @return 1 for success, 0 for failure
 
 * [AUTO-TRANSLATED:9f6ca231]
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
 * Input single frame H264 video, the starting byte of the frame can be 00 00 01, 00 00 00 01, please use mk_media_input_frame method
 * @param ctx Object pointer
 * @param data Single frame H264 data
 * @param len Number of bytes of single frame H264 data
 * @param dts Decode timestamp, unit milliseconds
 * @param pts Play timestamp, unit milliseconds
 * @return 1 for success, 0 for failure
 
 * [AUTO-TRANSLATED:3b96ace8]
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
 * Input single frame H265 video, the starting byte of the frame can be 00 00 01, 00 00 00 01, please use mk_media_input_frame method
 * @param ctx Object pointer
 * @param data Single frame H265 data
 * @param len Number of bytes of single frame H265 data
 * @param dts Decode timestamp, unit milliseconds
 * @param pts Play timestamp, unit milliseconds
 * @return 1 for success, 0 for failure
 
 * [AUTO-TRANSLATED:884739ba]
 */
API_EXPORT int API_CALL mk_media_input_h265(mk_media ctx, const void *data, int len, uint64_t dts, uint64_t pts);

/**
 * 输入YUV视频数据
 * @param ctx 对象指针
 * @param yuv yuv420p数据
 * @param linesize yuv420p linesize
 * @param cts 视频采集时间戳，单位毫秒
 * Input YUV video data
 * @param ctx Object pointer
 * @param yuv yuv420p data
 * @param linesize yuv420p linesize
 * @param cts Video capture timestamp, unit milliseconds
 
 * [AUTO-TRANSLATED:9c97805c]
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
 * Input single frame AAC audio (specify adts header separately), please use mk_media_input_frame method
 * @param ctx Object pointer
 * @param data Single frame AAC data without adts header, adts header 7 bytes
 * @param len Number of bytes of single frame AAC data
 * @param dts Timestamp, milliseconds
 * @param adts adts header, can be null
 * @return 1 for success, 0 for failure
 
 * [AUTO-TRANSLATED:11e0503d]
 */
API_EXPORT int API_CALL mk_media_input_aac(mk_media ctx, const void *data, int len, uint64_t dts, void *adts);

/**
 * 输入单帧PCM音频,启用ENABLE_FAAC编译时，该函数才有效
 * @param ctx 对象指针
 * @param data 单帧PCM数据
 * @param len 单帧PCM数据字节数
 * @param dts 时间戳，毫秒
 * @return 1代表成功，0失败
 * Input single frame PCM audio, this function is valid only when ENABLE_FAAC is compiled
 * @param ctx Object pointer
 * @param data Single frame PCM data
 * @param len Number of bytes of single frame PCM data
 * @param dts Timestamp, milliseconds
 * @return 1 for success, 0 for failure
 
 * [AUTO-TRANSLATED:70f7488b]
 */
API_EXPORT int API_CALL mk_media_input_pcm(mk_media ctx, void *data, int len, uint64_t pts);

/**
 * 输入单帧OPUS/G711音频帧，请改用mk_media_input_frame方法
 * @param ctx 对象指针
 * @param data 单帧音频数据
 * @param len  单帧音频数据字节数
 * @param dts 时间戳，毫秒
 * @return 1代表成功，0失败
 * Input single frame OPUS/G711 audio frame, please use mk_media_input_frame method
 * @param ctx Object pointer
 * @param data Single frame audio data
 * @param len  Number of bytes of single frame audio data
 * @param dts Timestamp, milliseconds
 * @return 1 for success, 0 for failure
 
 * [AUTO-TRANSLATED:4ffeabd6]
 */
API_EXPORT int API_CALL mk_media_input_audio(mk_media ctx, const void* data, int len, uint64_t dts);

/**
 * MediaSource.close()回调事件
 * 在选择关闭一个关联的MediaSource时，将会最终触发到该回调
 * 你应该通过该事件调用mk_media_release函数并且释放其他资源
 * 如果你不调用mk_media_release函数，那么MediaSource.close()操作将无效
 * @param user_data 用户数据指针，通过mk_media_set_on_close函数设置
 * MediaSource.close() callback event
 * When you choose to close an associated MediaSource, it will eventually trigger this callback
 * You should call mk_media_release function and release other resources through this event
 * If you do not call mk_media_release function, then the MediaSource.close() operation will be invalid
 * @param user_data User data pointer, set by mk_media_set_on_close function
 
 * [AUTO-TRANSLATED:20191b2d]
 */
typedef void(API_CALL *on_mk_media_close)(void *user_data);

/**
 * 监听MediaSource.close()事件
 * 在选择关闭一个关联的MediaSource时，将会最终触发到该回调
 * 你应该通过该事件调用mk_media_release函数并且释放其他资源
 * @param ctx 对象指针
 * @param cb 回调指针
 * @param user_data 用户数据指针
 * Listen to MediaSource.close() event
 * When you choose to close an associated MediaSource, it will eventually trigger this callback
 * You should call mk_media_release function and release other resources through this event
 * @param ctx Object pointer
 * @param cb Callback pointer
 * @param user_data User data pointer
 
 * [AUTO-TRANSLATED:35d9db20]
 */
API_EXPORT void API_CALL mk_media_set_on_close(mk_media ctx, on_mk_media_close cb, void *user_data);
API_EXPORT void API_CALL mk_media_set_on_close2(mk_media ctx, on_mk_media_close cb, void *user_data, on_user_data_free user_data_free);

/**
 * 收到客户端的seek请求时触发该回调
 * @param user_data 用户数据指针,通过mk_media_set_on_seek设置
 * @param stamp_ms seek至的时间轴位置，单位毫秒
 * @return 1代表将处理seek请求，0代表忽略该请求
 * Triggered when the client receives a seek request
 * @param user_data User data pointer, set by mk_media_set_on_seek
 * @param stamp_ms Seek to the timeline position, unit milliseconds
 * @return 1 means the seek request will be processed, 0 means the request will be ignored
 
 * [AUTO-TRANSLATED:c3301852]
 */
typedef int(API_CALL *on_mk_media_seek)(void *user_data,uint32_t stamp_ms);

/**
 * 收到客户端的pause或resume请求时触发该回调
 * @param user_data 用户数据指针,通过mk_media_set_on_pause设置
 * @param pause 1:暂停, 0: 恢复
 * Triggered when the client receives a pause or resume request
 * @param user_data User data pointer, set by mk_media_set_on_pause
 * @param pause 1: pause, 0: resume
 
 * [AUTO-TRANSLATED:4f8aa828]
 */
typedef int(API_CALL* on_mk_media_pause)(void* user_data, int pause);

/**
 * 收到客户端的speed请求时触发该回调
 * @param user_data 用户数据指针,通过mk_media_set_on_pause设置
 * @param speed 0.5 1.0 2.0
 * Triggered when the client receives a speed request
 * @param user_data User data pointer, set by mk_media_set_on_pause
 * @param speed 0.5 1.0 2.0
 
 * [AUTO-TRANSLATED:51bd090d]
 */
typedef int(API_CALL* on_mk_media_speed)(void* user_data, float speed);

/**
 * 监听播放器seek请求事件
 * @param ctx 对象指针
 * @param cb 回调指针
 * @param user_data 用户数据指针
 * Listen to player seek request event
 * @param ctx Object pointer
 * @param cb Callback pointer
 * @param user_data User data pointer
 
 * [AUTO-TRANSLATED:50c723d0]
 */
API_EXPORT void API_CALL mk_media_set_on_seek(mk_media ctx, on_mk_media_seek cb, void *user_data);
API_EXPORT void API_CALL mk_media_set_on_seek2(mk_media ctx, on_mk_media_seek cb, void *user_data, on_user_data_free user_data_free);

/**
 * 监听播放器pause请求事件
 * @param ctx 对象指针
 * @param cb 回调指针
 * @param user_data 用户数据指针
 * Listen to player pause request event
 * @param ctx Object pointer
 * @param cb Callback pointer
 * @param user_data User data pointer
 
 * [AUTO-TRANSLATED:bd6e9068]
 */
API_EXPORT void API_CALL mk_media_set_on_pause(mk_media ctx, on_mk_media_pause cb, void *user_data);
API_EXPORT void API_CALL mk_media_set_on_pause2(mk_media ctx, on_mk_media_pause cb, void *user_data, on_user_data_free user_data_free);

/**
 * 监听播放器pause请求事件
 * @param ctx 对象指针
 * @param cb 回调指针
 * @param user_data 用户数据指针
 * Listen to player pause request event
 * @param ctx Object pointer
 * @param cb Callback pointer
 * @param user_data User data pointer
 
 * [AUTO-TRANSLATED:bd6e9068]
 */
API_EXPORT void API_CALL mk_media_set_on_speed(mk_media ctx, on_mk_media_speed cb, void *user_data);
API_EXPORT void API_CALL mk_media_set_on_speed2(mk_media ctx, on_mk_media_speed cb, void *user_data, on_user_data_free user_data_free);

/**
 * 获取总的观看人数
 * @param ctx 对象指针
 * @return 观看人数
 * Get the total number of viewers
 * @param ctx Object pointer
 * @return Number of viewers
 
 * [AUTO-TRANSLATED:56635caf]
 */
API_EXPORT int API_CALL mk_media_total_reader_count(mk_media ctx);

/**
 * 生成的MediaSource注册或注销事件
 * @param user_data 设置回调时的用户数据指针
 * @param sender 生成的MediaSource对象
 * @param regist 1为注册事件，0为注销事件
 * MediaSource registration or deregistration event
 * @param user_data User data pointer set when setting the callback
 * @param sender Generated MediaSource object
 * @param regist 1 for registration event, 0 for deregistration event
 
 * [AUTO-TRANSLATED:4585bbef]
 */
typedef void(API_CALL *on_mk_media_source_regist)(void *user_data, mk_media_source sender, int regist);

/**
 * 设置MediaSource注册或注销事件回调函数
 * @param ctx 对象指针
 * @param cb 回调指针
 * @param user_data 用户数据指针
 * Set MediaSource registration or deregistration event callback function
 * @param ctx Object pointer
 * @param cb Callback pointer
 * @param user_data User data pointer
 
 * [AUTO-TRANSLATED:1c3b45be]
 */
API_EXPORT void API_CALL mk_media_set_on_regist(mk_media ctx, on_mk_media_source_regist cb, void *user_data);
API_EXPORT void API_CALL mk_media_set_on_regist2(mk_media ctx, on_mk_media_source_regist cb, void *user_data, on_user_data_free user_data_free);

/**
 * rtp推流成功与否的回调(第一次成功后，后面将一直重试)
 * Callback for whether rtp streaming is successful or not (after the first success, it will retry continuously)
 
 * [AUTO-TRANSLATED:7e00f7fb]
 */
typedef on_mk_media_source_send_rtp_result on_mk_media_send_rtp_result;

/**
 * 开始发送一路ps-rtp流(通过ssrc区分多路)，此api线程安全
 * @param ctx 对象指针
 * @param dst_url 目标ip或域名
 * @param dst_port 目标端口
 * @param ssrc rtp的ssrc，10进制的字符串打印
 * @param con_type 0: tcp主动，1：udp主动，2：tcp被动，3：udp被动
 * @param options 选项
 * @param cb 启动成功或失败回调
 * @param user_data 回调用户指针
 * Start sending a ps-rtp stream (distinguished by ssrc), this api is thread-safe
 * @param ctx Object pointer
 * @param dst_url Target ip or domain name
 * @param dst_port Target port
 * @param ssrc rtp's ssrc, 10-base string print
 * @param con_type 0: tcp active, 1: udp active, 2: tcp passive, 3: udp passive
 * @param options Options
 * @param cb Start success or failure callback
 * @param user_data Callback user pointer
 
 * [AUTO-TRANSLATED:dbf694a0]
 */
API_EXPORT void API_CALL mk_media_start_send_rtp(mk_media ctx, const char *dst_url, uint16_t dst_port, const char *ssrc, int con_type, on_mk_media_send_rtp_result cb, void *user_data);
API_EXPORT void API_CALL mk_media_start_send_rtp2(mk_media ctx, const char *dst_url, uint16_t dst_port, const char *ssrc, int con_type, on_mk_media_send_rtp_result cb, void *user_data, on_user_data_free user_data_free);
API_EXPORT void API_CALL mk_media_start_send_rtp3(mk_media ctx, const char *dst_url, uint16_t dst_port, const char *ssrc, int con_type, mk_ini options, on_mk_media_send_rtp_result cb, void *user_data);
API_EXPORT void API_CALL mk_media_start_send_rtp4(mk_media ctx, const char *dst_url, uint16_t dst_port, const char *ssrc, int con_type, mk_ini options, on_mk_media_send_rtp_result cb, void *user_data,on_user_data_free user_data_free);
/**
 * 停止某路或全部ps-rtp发送，此api线程安全
 * @param ctx 对象指针
 * @param ssrc rtp的ssrc，10进制的字符串打印，如果为null或空字符串，则停止所有rtp推流
 * Stop a certain route or all ps-rtp sending, this api is thread-safe
 * @param ctx Object pointer
 * @param ssrc rtp's ssrc, 10-base string print, if it is null or empty string, stop all rtp streaming
 
 * [AUTO-TRANSLATED:6fb2b1df]
 */
API_EXPORT void API_CALL mk_media_stop_send_rtp(mk_media ctx, const char *ssrc);

/**
 * 获取所属线程
 * @param ctx 对象指针
 * Get the belonging thread
 * @param ctx Object pointer
 
 
 
 * [AUTO-TRANSLATED:85a157e8]
 */
API_EXPORT mk_thread API_CALL mk_media_get_owner_thread(mk_media ctx);


#ifdef __cplusplus
}
#endif

#endif /* MK_MEDIA_H_ */
