/*
 * Copyright (c) 2016 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/xia-chu/ZLMediaKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#ifndef ZLMEDIAKIT_MK_H264_SPLITTER_H
#define ZLMEDIAKIT_MK_H264_SPLITTER_H

#include "mk_common.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef void *mk_h264_splitter;

/**
 * h264 分帧器输出回调函数
 * @param user_data 设置回调时的用户数据指针
 * @param splitter 对象
 * @param frame 帧数据
 * @param size 帧数据长度
 */
typedef void(API_CALL *on_mk_h264_splitter_frame)(void *user_data, mk_h264_splitter splitter, const char *frame, int size);

/**
 * 创建h264分帧器
 * @param cb 分帧回调函数
 * @param user_data 回调用户数据指针
 * @return 分帧器对象
 */
API_EXPORT mk_h264_splitter API_CALL mk_h264_splitter_create(on_mk_h264_splitter_frame cb, void *user_data);

/**
 * 删除h264分帧器
 * @param ctx 分帧器
 */
API_EXPORT void API_CALL mk_h264_splitter_release(mk_h264_splitter ctx);

/**
 * 输入数据并分帧
 * @param ctx 分帧器
 * @param data h264/h265数据
 * @param size 数据长度
 */
API_EXPORT void API_CALL mk_h264_splitter_input_data(mk_h264_splitter ctx, const char *data, int size);

#ifdef __cplusplus
}
#endif
#endif //ZLMEDIAKIT_MK_H264_SPLITTER_H
