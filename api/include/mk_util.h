/*
 * Copyright (c) 2016 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/xiongziliang/ZLMediaKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MK_UTIL_H
#define MK_UTIL_H

#include <stdlib.h>
#include "mk_common.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * 获取本程序可执行文件路径
 * @return 文件路径，使用完后需要自己free
 */
API_EXPORT char* API_CALL mk_util_get_exe_path();

/**
 * 获取本程序可执行文件相同目录下文件的绝对路径
 * @param relative_path 同目录下文件的路径相对,可以为null
 * @return 文件路径，使用完后需要自己free
 */
API_EXPORT char* API_CALL mk_util_get_exe_dir(const char *relative_path);

/**
 * 获取unix标准的系统时间戳
 * @return 当前系统时间戳
 */
API_EXPORT uint64_t API_CALL mk_util_get_current_millisecond();

/**
 * 获取时间字符串
 * @param fmt 时间格式，譬如%Y-%m-%d %H:%M:%S
 * @return 时间字符串，使用完后需要自己free
 */
API_EXPORT char* API_CALL mk_util_get_current_time_string(const char *fmt);

/**
 * 打印二进制为字符串
 * @param buf 二进制数据
 * @param len 数据长度
 * @return 可打印的调试信息，使用完后需要自己free
 */
API_EXPORT char* API_CALL mk_util_hex_dump(const void *buf, int len);
///////////////////////////////////////////日志/////////////////////////////////////////////

/**
 * 打印日志
 * @param level 日志级别,支持0~4
 * @param file __FILE__
 * @param function __FUNCTION__
 * @param line __LINE__
 * @param fmt printf类型的格式控制字符串
 * @param ... 不定长参数
 */
API_EXPORT void API_CALL mk_log_printf(int level, const char *file, const char *function, int line, const char *fmt, ...);

// 以下宏可以替换printf使用
#define log_trace(fmt,...) mk_log_printf(0,__FILE__,__FUNCTION__,__LINE__,fmt,##__VA_ARGS__)
#define log_debug(fmt,...) mk_log_printf(1,__FILE__,__FUNCTION__,__LINE__,fmt,##__VA_ARGS__)
#define log_info(fmt,...) mk_log_printf(2,__FILE__,__FUNCTION__,__LINE__,fmt,##__VA_ARGS__)
#define log_warn(fmt,...) mk_log_printf(3,__FILE__,__FUNCTION__,__LINE__,fmt,##__VA_ARGS__)
#define log_error(fmt,...) mk_log_printf(4,__FILE__,__FUNCTION__,__LINE__,fmt,##__VA_ARGS__)
#define log_printf(lev,fmt,...) mk_log_printf(lev,__FILE__,__FUNCTION__,__LINE__,fmt,##__VA_ARGS__)

#ifdef __cplusplus
}
#endif
#endif //MK_UTIL_H
