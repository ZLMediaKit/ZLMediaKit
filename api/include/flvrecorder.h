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

#ifndef MK_FLV_RECORDER_API_H_
#define MK_FLV_RECORDER_API_H_

#include "common.h"

#ifdef __cplusplus
extern "C" {
#endif

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
 * @param app 绑定的RtmpMediaSource的 app名
 * @param stream 绑定的RtmpMediaSource的 stream名
 * @param file_path 文件存放地址
 * @return 0:开始超过，-1:失败,打开文件失败或该RtmpMediaSource不存在
 */
API_EXPORT int API_CALL mk_flv_recorder_start(mk_flv_recorder ctx, const char *app, const char *stream, const char *file_path);

#ifdef __cplusplus
}
#endif

#endif /* MK_FLV_RECORDER_API_H_ */
