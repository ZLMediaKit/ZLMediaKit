/*
 * Copyright (c) 2016-present The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/ZLMediaKit/ZLMediaKit).
 *
 * Use of this source code is governed by MIT-like license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MK_RECORDER_API_H_
#define MK_RECORDER_API_H_

#include "mk_common.h"
#include "mk_util.h"

#ifdef __cplusplus
extern "C" {
#endif

// /////////////////////////////////////////flv录制/////////////////////////////////////////////  [AUTO-TRANSLATED:a084663f]
// /////////////////////////////////////////flv录制/////////////////////////////////////////////

typedef struct mk_flv_recorder_t *mk_flv_recorder;

/**
 * 创建flv录制器
 * @return
 * Create flv recorder
 * @return
 
 * [AUTO-TRANSLATED:7582cde1]
 */
API_EXPORT mk_flv_recorder API_CALL mk_flv_recorder_create();

/**
 * 释放flv录制器
 * @param ctx
 * Release flv recorder
 * @param ctx
 
 * [AUTO-TRANSLATED:c33c76bb]
 */
API_EXPORT void API_CALL mk_flv_recorder_release(mk_flv_recorder ctx);

/**
 * 开始录制flv
 * @param ctx flv录制器
 * @param vhost 虚拟主机
 * @param app 绑定的RtmpMediaSource的 app名
 * @param stream 绑定的RtmpMediaSource的 stream名
 * @param file_path 文件存放地址
 * @return 0:开始超过，-1:失败,打开文件失败或该RtmpMediaSource不存在
 * Start recording flv
 * @param ctx flv recorder
 * @param vhost virtual host
 * @param app app name of the bound RtmpMediaSource
 * @param stream stream name of the bound RtmpMediaSource
 * @param file_path file storage address
 * @return 0: start exceeds, -1: failure, file opening fails or the RtmpMediaSource does not exist
 
 * [AUTO-TRANSLATED:194cf3de]
 */
API_EXPORT int API_CALL mk_flv_recorder_start(mk_flv_recorder ctx, const char *vhost, const char *app, const char *stream, const char *file_path);

// /////////////////////////////////////////hls/mp4录制/////////////////////////////////////////////  [AUTO-TRANSLATED:99c61c68]
// /////////////////////////////////////////hls/mp4录制/////////////////////////////////////////////

/**
 * 获取录制状态
 * @param type 0:hls,1:MP4
 * @param vhost 虚拟主机
 * @param app 应用名
 * @param stream 流id
 * @return 录制状态,0:未录制, 1:正在录制
 * Get recording status
 * @param type 0: hls, 1: MP4
 * @param vhost virtual host
 * @param app application name
 * @param stream stream id
 * @return recording status, 0: not recording, 1: recording
 
 * [AUTO-TRANSLATED:0b1d374a]
 */
API_EXPORT int API_CALL mk_recorder_is_recording(int type, const char *vhost, const char *app, const char *stream);

/**
 * 开始录制
 * @param type 0:hls-ts,1:MP4,2:hls-fmp4,3:http-fmp4,4:http-ts
 * @param vhost 虚拟主机
 * @param app 应用名
 * @param stream 流id
 * @param customized_path 录像文件保存自定义目录，默认为空或null则自动生成
 * @param max_second mp4录制最大切片时间，单位秒，置0则采用配置文件配置
 * @return 1代表成功，0代表失败
 * Start recording
 * @param type 0: hls-ts, 1: MP4, 2: hls-fmp4, 3: http-fmp4, 4: http-ts
 * @param vhost virtual host
 * @param app application name
 * @param stream stream id
 * @param customized_path custom directory for saving recording files, defaults to empty or null, automatically generated
 * @param max_second maximum slice time for mp4 recording, in seconds, set to 0 to use the configuration file configuration
 * @return 1 represents success, 0 represents failure
 
 * [AUTO-TRANSLATED:0a1c8c3e]
 */
API_EXPORT int API_CALL mk_recorder_start(int type, const char *vhost, const char *app, const char *stream, const char *customized_path, size_t max_second);

/**
 * 停止录制
 * @param type 0:hls-ts,1:MP4,2:hls-fmp4,3:http-fmp4,4:http-ts
 * @param vhost 虚拟主机
 * @param app 应用名
 * @param stream 流id
 * @return 1:成功，0：失败
 * Stop recording
 * @param type 0: hls-ts, 1: MP4, 2: hls-fmp4, 3: http-fmp4, 4: http-ts
 * @param vhost virtual host
 * @param app application name
 * @param stream stream id
 * @return 1: success, 0: failure
 
 
 * [AUTO-TRANSLATED:df1638e7]
 */
API_EXPORT int API_CALL mk_recorder_stop(int type, const char *vhost, const char *app, const char *stream);

/**
 * 加载mp4列表
 * @param vhost 虚拟主机
 * @param app app
 * @param stream 流id
 * @param file_path 文件路径
 * @param file_repeat 循环解复用
 * @param ini 配置
 */
API_EXPORT void API_CALL mk_load_mp4_file(const char *vhost, const char *app, const char *stream, const char *file_path, int file_repeat);
API_EXPORT void API_CALL mk_load_mp4_file2(const char *vhost, const char *app, const char *stream, const char *file_path, int file_repeat, mk_ini ini);

#ifdef __cplusplus
}
#endif

#endif /* MK_RECORDER_API_H_ */
