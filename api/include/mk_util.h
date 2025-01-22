/*
 * Copyright (c) 2016-present The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/ZLMediaKit/ZLMediaKit).
 *
 * Use of this source code is governed by MIT-like license that can be found in the
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
 * 释放mk api内部malloc的资源
 * Release resources allocated by mk api internally
 
 * [AUTO-TRANSLATED:92ecfef5]
 */
API_EXPORT void API_CALL mk_free(void *ptr);

/**
 * 获取本程序可执行文件路径
 * @return 文件路径，使用完后需要自己mk_free
 * Get the path of the executable file of this program
 * @return File path, need to be mk_free after use
 
 * [AUTO-TRANSLATED:5f1fae7f]
 */
API_EXPORT char* API_CALL mk_util_get_exe_path();

/**
 * 获取本程序可执行文件相同目录下文件的绝对路径
 * @param relative_path 同目录下文件的路径相对,可以为null
 * @return 文件路径，使用完后需要自己mk_free
 * Get the absolute path of the file in the same directory as the executable file of this program
 * @param relative_path The path of the file in the same directory, can be null
 * @return File path, need to be mk_free after use
 
 * [AUTO-TRANSLATED:80442e8e]
 */
API_EXPORT char* API_CALL mk_util_get_exe_dir(const char *relative_path);

/**
 * 获取unix标准的系统时间戳
 * @return 当前系统时间戳
 * Get the Unix standard system timestamp
 * @return Current system timestamp
 
 * [AUTO-TRANSLATED:feddaa5b]
 */
API_EXPORT uint64_t API_CALL mk_util_get_current_millisecond();

/**
 * 获取时间字符串
 * @param fmt 时间格式，譬如%Y-%m-%d %H:%M:%S
 * @return 时间字符串，使用完后需要自己mk_free
 * Get the time string
 * @param fmt Time format, such as %Y-%m-%d %H:%M:%S
 * @return Time string, need to be mk_free after use
 
 * [AUTO-TRANSLATED:c5a6c984]
 */
API_EXPORT char* API_CALL mk_util_get_current_time_string(const char *fmt);

/**
 * 打印二进制为字符串
 * @param buf 二进制数据
 * @param len 数据长度
 * @return 可打印的调试信息，使用完后需要自己mk_free
 * Print binary data as string
 * @param buf Binary data
 * @param len Data length
 * @return Printable debug information, need to be mk_free after use
 
 * [AUTO-TRANSLATED:5b76b3a5]
 */
API_EXPORT char* API_CALL mk_util_hex_dump(const void *buf, int len);

///////////////////////////////////////////mk ini/////////////////////////////////////////////
typedef struct mk_ini_t *mk_ini;

/**
 * 创建ini配置对象
 * Create ini configuration object
 
 * [AUTO-TRANSLATED:b8ce40cc]
 */
API_EXPORT mk_ini API_CALL mk_ini_create();

/**
 * 返回全局默认ini配置
 * @return 全局默认ini配置，请勿用mk_ini_release释放它
 * Return the global default ini configuration
 * @return Global default ini configuration, do not use mk_ini_release to release it
 
 * [AUTO-TRANSLATED:057ea031]
 */
API_EXPORT mk_ini API_CALL mk_ini_default();

/**
 * 加载ini配置文件内容
 * @param ini ini对象
 * @param str 配置文件内容
 * Load ini configuration file content
 * @param ini Ini object
 * @param str Configuration file content
 
 * [AUTO-TRANSLATED:b9366be5]
 */
API_EXPORT void API_CALL mk_ini_load_string(mk_ini ini, const char *str);

/**
 * 加载ini配置文件
 * @param ini ini对象
 * @param file 配置文件路径
 * Load ini configuration file
 * @param ini Ini object
 * @param file Configuration file path
 
 * [AUTO-TRANSLATED:688e0471]
 */
API_EXPORT void API_CALL mk_ini_load_file(mk_ini ini, const char *file);

/**
 * 销毁ini配置对象
 * Destroy ini configuration object
 
 * [AUTO-TRANSLATED:b6286ab8]
 */
API_EXPORT void API_CALL mk_ini_release(mk_ini ini);

/**
 * 添加或覆盖配置项
 * @param ini 配置对象
 * @param key 配置名，两段式，如：field.key
 * @param value 配置值
 * Add or overwrite configuration item
 * @param ini Configuration object
 * @param key Configuration name, two-part, such as: field.key
 * @param value Configuration value
 
 * [AUTO-TRANSLATED:1b2c8bfa]
 */
API_EXPORT void API_CALL mk_ini_set_option(mk_ini ini, const char *key, const char *value);
API_EXPORT void API_CALL mk_ini_set_option_int(mk_ini ini, const char *key, int value);

/**
 * 获取配置项
 * @param ini 配置对象
 * @param key 配置名，两段式，如：field.key
 * @return 配置不存在返回NULL，否则返回配置值
 * Get configuration item
 * @param ini Configuration object
 * @param key Configuration name, two-part, such as: field.key
 * @return NULL if the configuration does not exist, otherwise return the configuration value
 
 * [AUTO-TRANSLATED:4df4bc65]
 */
API_EXPORT const char *API_CALL mk_ini_get_option(mk_ini ini, const char *key);

/**
 * 删除配置项
 * @param ini 配置对象
 * @param key 配置名，两段式，如：field.key
 * @return 1: 成功，0: 该配置不存在
 * Delete configuration item
 * @param ini Configuration object
 * @param key Configuration name, two-part, such as: field.key
 * @return 1: Success, 0: The configuration does not exist
 
 * [AUTO-TRANSLATED:dbbbdca3]
 */
API_EXPORT int API_CALL mk_ini_del_option(mk_ini ini, const char *key);

/**
 * 导出为配置文件内容
 * @param ini 配置对象
 * @return 配置文件内容字符串，用完后需要自行mk_free
 * Export to configuration file content
 * @param ini Configuration object
 * @return Configuration file content string, need to be mk_free after use
 
 * [AUTO-TRANSLATED:94620b68]
 */
API_EXPORT char *API_CALL mk_ini_dump_string(mk_ini ini);

/**
 * 导出配置文件到文件
 * @param ini 配置对象
 * @param file 配置文件路径
 * Export configuration file to file
 * @param ini Configuration object
 * @param file Configuration file path
 
 * [AUTO-TRANSLATED:8fac58af]
 */
API_EXPORT void API_CALL mk_ini_dump_file(mk_ini ini, const char *file);
// /////////////////////////////////////////统计/////////////////////////////////////////////  [AUTO-TRANSLATED:964becb9]
// /////////////////////////////////////////统计/////////////////////////////////////////////

typedef void(API_CALL *on_mk_get_statistic_cb)(void *user_data, mk_ini ini);

/**
 * 获取内存数据统计
 * @param ini 存放统计结果
 * Get memory data statistics
 * @param ini Store statistical results
 
 * [AUTO-TRANSLATED:48d8035c]
 */
API_EXPORT void API_CALL mk_get_statistic(on_mk_get_statistic_cb cb, void *user_data, on_user_data_free free_cb);

// /////////////////////////////////////////日志/////////////////////////////////////////////  [AUTO-TRANSLATED:b1bd4de8]
// /////////////////////////////////////////日志/////////////////////////////////////////////

/**
 * 打印日志
 * @param level 日志级别,支持0~4
 * @param file __FILE__
 * @param function __FUNCTION__
 * @param line __LINE__
 * @param fmt printf类型的格式控制字符串
 * @param ... 不定长参数
 * Print log
 * @param level Log level, support 0~4
 * @param file __FILE__
 * @param function __FUNCTION__
 * @param line __LINE__
 * @param fmt printf type format control string
 * @param ... Variable length parameters
 
 * [AUTO-TRANSLATED:f19956e7]
 */
API_EXPORT void API_CALL mk_log_printf(int level, const char *file, const char *function, int line, const char *fmt, ...);

// 以下宏可以替换printf使用  [AUTO-TRANSLATED:73b59437]
// The following macros can replace printf
#define log_printf(lev, ...) mk_log_printf(lev, __FILE__, __FUNCTION__, __LINE__, ##__VA_ARGS__)
#define log_trace(...) log_printf(0, ##__VA_ARGS__)
#define log_debug(...) log_printf(1, ##__VA_ARGS__)
#define log_info(...) log_printf(2, ##__VA_ARGS__)
#define log_warn(...) log_printf(3, ##__VA_ARGS__)
#define log_error(...) log_printf(4, ##__VA_ARGS__)

#ifdef __cplusplus
}
#endif
#endif //MK_UTIL_H
