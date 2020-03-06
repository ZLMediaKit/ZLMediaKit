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


///////////////////////////////////////////hls/mp4录制/////////////////////////////////////////////

/**
 * 获取录制状态
 * @param type 0:hls,1:MP4
 * @param vhost 虚拟主机
 * @param app 应用名
 * @param stream 流id
 * @return 录制状态,0:未录制,1:等待MediaSource注册，注册成功后立即开始录制,2:MediaSource已注册，并且正在录制
 */
API_EXPORT int API_CALL mk_recorder_status(int type, const char *vhost, const char *app, const char *stream);

/**
 * 开始录制
 * @param type 0:hls,1:MP4
 * @param vhost 虚拟主机
 * @param app 应用名
 * @param stream 流id
 * @param customized_path 录像文件保存自定义目录，默认为空或null则自动生成
 * @param wait_for_record 是否等待流注册后再录制，未注册时，置false将返回失败
 * @param continue_record 流注销时是否继续等待录制还是立即停止录制
 * @return 0代表成功，负数代表失败
 */
API_EXPORT int API_CALL mk_recorder_start(int type, const char *vhost, const char *app, const char *stream, const char *customized_path, int wait_for_record, int continue_record);

/**
 * 停止录制
 * @param type 0:hls,1:MP4
 * @param vhost 虚拟主机
 * @param app 应用名
 * @param stream 流id
 * @return 1:成功，0：失败
 */
API_EXPORT int API_CALL mk_recorder_stop(int type, const char *vhost, const char *app, const char *stream);

/**
 * 停止所有录制，一般程序退出时调用
 */
API_EXPORT void API_CALL mk_recorder_stop_all();

#ifdef __cplusplus
}
#endif

#endif /* MK_RECORDER_API_H_ */
