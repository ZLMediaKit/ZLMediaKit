/*
 * Copyright (c) 2016 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/xiongziliang/ZLMediaKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MK_COMMON_H
#define MK_COMMON_H

#include <stdint.h>

#if defined(_WIN32)

#ifndef MediaKitApi_STATIC
#if defined(MediaKitApi_EXPORTS)
        #define API_EXPORT __declspec(dllexport)
    #else
        #define API_EXPORT __declspec(dllimport)
    #endif

    #define API_CALL __cdecl
#else
#define API_EXPORT
#define API_CALL
#endif

#else
#define API_EXPORT
#define API_CALL
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    // 线程数
    int thread_num;

    // 日志级别,支持0~4
    int log_level;
    //文件日志保存路径,路径可以不存在(内部可以创建文件夹)，设置为NULL关闭日志输出至文件
    const char *log_file_path;
    //文件日志保存天数,设置为0关闭日志文件
    int log_file_days;

    // 配置文件是内容还是路径
    int ini_is_path;
    // 配置文件内容或路径，可以为NULL,如果该文件不存在，那么将导出默认配置至该文件
    const char *ini;

    // ssl证书是内容还是路径
    int ssl_is_path;
    // ssl证书内容或路径，可以为NULL
    const char *ssl;
    // 证书密码，可以为NULL
    const char *ssl_pwd;
} mk_config;

/**
 * 初始化环境，调用该库前需要先调用此函数
 * @param cfg 库运行相关参数
 */
API_EXPORT void API_CALL mk_env_init(const mk_config *cfg);

/**
 * 关闭所有服务器，请在main函数退出时调用
 */
API_EXPORT void API_CALL mk_stop_all_server();

/**
 * 基础类型参数版本的mk_env_init，为了方便其他语言调用
 * @param thread_num 线程数
 * @param log_level 日志级别,支持0~4
 * @param log_file_path 文件日志保存路径,路径可以不存在(内部可以创建文件夹)，设置为NULL关闭日志输出至文件
 * @param log_file_days 文件日志保存天数,设置为0关闭日志文件
 * @param ini_is_path 配置文件是内容还是路径
 * @param ini 配置文件内容或路径，可以为NULL,如果该文件不存在，那么将导出默认配置至该文件
 * @param ssl_is_path ssl证书是内容还是路径
 * @param ssl ssl证书内容或路径，可以为NULL
 * @param ssl_pwd 证书密码，可以为NULL
 */
API_EXPORT void API_CALL mk_env_init1(int thread_num,
                                      int log_level,
                                      const char *log_file_path,
                                      int log_file_days,
                                      int ini_is_path,
                                      const char *ini,
                                      int ssl_is_path,
                                      const char *ssl,
                                      const char *ssl_pwd);

/**
 * 设置配置项
 * @param key 配置项名
 * @param val 配置项值
 */
API_EXPORT void API_CALL mk_set_option(const char *key, const char *val);

/**
 * 创建http[s]服务器
 * @param port htt监听端口，推荐80，传入0则随机分配
 * @param ssl 是否为ssl类型服务器
 * @return 0:失败,非0:端口号
 */
API_EXPORT uint16_t API_CALL mk_http_server_start(uint16_t port, int ssl);

/**
 * 创建rtsp[s]服务器
 * @param port rtsp监听端口，推荐554，传入0则随机分配
 * @param ssl 是否为ssl类型服务器
 * @return 0:失败,非0:端口号
 */
API_EXPORT uint16_t API_CALL mk_rtsp_server_start(uint16_t port, int ssl);

/**
 * 创建rtmp[s]服务器
 * @param port rtmp监听端口，推荐1935，传入0则随机分配
 * @param ssl 是否为ssl类型服务器
 * @return 0:失败,非0:端口号
 */
API_EXPORT uint16_t API_CALL mk_rtmp_server_start(uint16_t port, int ssl);

/**
 * 创建rtp服务器
 * @param port rtp监听端口(包括udp/tcp)
 * @return 0:失败,非0:端口号
 */
API_EXPORT uint16_t API_CALL mk_rtp_server_start(uint16_t port);

/**
 * 创建shell服务器
 * @param port shell监听端口
 * @return 0:失败,非0:端口号
 */
API_EXPORT uint16_t API_CALL mk_shell_server_start(uint16_t port);

#ifdef __cplusplus
}
#endif


#endif /* MK_COMMON_H */
