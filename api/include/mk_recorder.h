/*
 * Copyright (c) 2016 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/xiongziliang/ZLMediaKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MK_RECORDER_API_H_
#define MK_RECORDER_API_H_

#include "mk_common.h"

#ifdef __cplusplus
extern "C" {
#endif

///////////////////////////////////////////flv录制/////////////////////////////////////////////

typedef void* mk_flv_recorder;

/**
 * 创建flv录制器
 * @return
 */
API_EXPORT mk_flv_recorder API_CALL mk_flv_recorder_create();

/**
 * 释放flv录制器
 * @param ctx
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
 */
API_EXPORT int API_CALL mk_flv_recorder_start(mk_flv_recorder ctx, const char *vhost, const char *app, const char *stream, const char *file_path);

#ifdef __cplusplus
}
#endif

#endif /* MK_RECORDER_API_H_ */
