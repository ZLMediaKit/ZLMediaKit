/*
 * Copyright (c) 2016 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/xia-chu/ZLMediaKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#ifndef ZLMEDIAKIT_MK_FRAME_H
#define ZLMEDIAKIT_MK_FRAME_H

#include "mk_common.h"

#ifdef __cplusplus
extern "C" {
#endif

//是否为关键帧
#define MK_FRAME_FLAG_IS_KEY (1 << 0)
//是否为配置帧(sps/pps/vps等)
#define MK_FRAME_FLAG_IS_CONFIG (1 << 1)
//是否可丢弃的帧(sei/aud)
#define MK_FRAME_FLAG_DROP_ABLE  (1 << 2)
//是否不可单独解码的帧(多slice的非vcl帧)
#define MK_FRAME_FLAG_NOT_DECODE_ABLE (1 << 3)

//codec id常量定义
API_EXPORT extern const int MKCodecH264;
API_EXPORT extern const int MKCodecH265;
API_EXPORT extern const int MKCodecAAC;
API_EXPORT extern const int MKCodecG711A;
API_EXPORT extern const int MKCodecG711U;
API_EXPORT extern const int MKCodecOpus;
API_EXPORT extern const int MKCodecL16;
API_EXPORT extern const int MKCodecVP8;
API_EXPORT extern const int MKCodecVP9;
API_EXPORT extern const int MKCodecAV1;
API_EXPORT extern const int MKCodecJPEG;

typedef struct mk_frame_t *mk_frame;

// 用户自定义free回调函数
typedef void(API_CALL *on_mk_frame_data_release)(void *user_data, char *ptr);

/**
 * 创建frame对象，并返回其引用
 * @param codec_id 编解码类型，请参考MKCodecXXX定义
 * @param dts 解码时间戳，单位毫秒
 * @param pts 显示时间戳，单位毫秒
 * @param data 单帧数据
 * @param size 单帧数据长度
 * @param cb data指针free释放回调, 如果为空，内部会拷贝数据
 * @param user_data data指针free释放回调用户指针
 * @return frame对象引用
 */
API_EXPORT mk_frame API_CALL mk_frame_create(int codec_id, uint64_t dts, uint64_t pts, const char *data, size_t size,
                                            on_mk_frame_data_release cb, void *user_data);
API_EXPORT mk_frame API_CALL mk_frame_create2(int codec_id, uint64_t dts, uint64_t pts, const char *data, size_t size,
                                             on_mk_frame_data_release cb, void *user_data, on_user_data_free user_data_free);
/**
 * 减引用frame对象
 * @param frame 帧对象引用
 */
API_EXPORT void API_CALL mk_frame_unref(mk_frame frame);

/**
 * 引用frame对象
 * @param frame 被引用的frame对象
 * @return 新的对象引用
 */
API_EXPORT mk_frame API_CALL mk_frame_ref(mk_frame frame);

/**
 * 获取frame 编码codec类型，请参考MKCodecXXX定义
 */
API_EXPORT int API_CALL mk_frame_codec_id(mk_frame frame);

/**
 * 获取帧编码codec名称
 */
API_EXPORT const char* API_CALL mk_frame_codec_name(mk_frame frame);

/**
 * 帧是否为视频
 */
API_EXPORT int API_CALL mk_frame_is_video(mk_frame frame);

/**
 * 获取帧数据指针
 */
API_EXPORT const char* API_CALL mk_frame_get_data(mk_frame frame);

/**
 * 获取帧数据指针长度
 */
API_EXPORT size_t API_CALL mk_frame_get_data_size(mk_frame frame);

/**
 * 返回帧数据前缀长度，譬如H264/H265前缀一般是0x00 00 00 01,那么本函数返回4
 */
API_EXPORT size_t API_CALL mk_frame_get_data_prefix_size(mk_frame frame);

/**
 * 获取解码时间戳，单位毫秒
 */
API_EXPORT uint64_t API_CALL mk_frame_get_dts(mk_frame frame);

/**
 * 获取显示时间戳，单位毫秒
 */
API_EXPORT uint64_t API_CALL mk_frame_get_pts(mk_frame frame);

/**
 * 获取帧flag，请参考 MK_FRAME_FLAG
 */
API_EXPORT uint32_t API_CALL mk_frame_get_flags(mk_frame frame);

//////////////////////////////////////////////////////////////////////

typedef struct mk_buffer_t *mk_buffer;
typedef struct mk_frame_merger_t *mk_frame_merger;

/**
 * 创建帧合并器
 * @param type 起始头类型，0: none, 1: h264_prefix/AnnexB(0x 00 00 00 01), 2: mp4_nal_size(avcC)
 * @return 帧合并器
 */
API_EXPORT mk_frame_merger API_CALL mk_frame_merger_create(int type);

/**
 * 销毁帧合并器
 * @param ctx 对象指针
 */
API_EXPORT void API_CALL mk_frame_merger_release(mk_frame_merger ctx);

/**
 * 清空merger对象缓冲，方便复用
 * @param ctx 对象指针
 */
API_EXPORT void API_CALL mk_frame_merger_clear(mk_frame_merger ctx);

/**
 * 合并帧回调函数
 * @param user_data 用户数据指针
 * @param dts 解码时间戳
 * @param pts 显示时间戳
 * @param buffer 合并后数据buffer对象
 * @param have_key_frame 合并后数据中是否包含关键帧
 */
typedef void(API_CALL *on_mk_frame_merger)(void *user_data, uint64_t dts, uint64_t pts, mk_buffer buffer, int have_key_frame);

/**
 * 输入frame到merger对象并合并
 * @param ctx 对象指针
 * @param frame 帧数据
 * @param cb 帧合并回调函数
 * @param user_data 帧合并回调函数用户数据指针
 */
API_EXPORT void API_CALL mk_frame_merger_input(mk_frame_merger ctx, mk_frame frame, on_mk_frame_merger cb, void *user_data);

/**
 * 强制flush merger对象缓冲，调用此api前需要确保先调用mk_frame_merger_input函数并且回调参数有效
 * @param ctx 对象指针
 */
API_EXPORT void API_CALL mk_frame_merger_flush(mk_frame_merger ctx);

#ifdef __cplusplus
}
#endif

#endif //ZLMEDIAKIT_MK_FRAME_H
