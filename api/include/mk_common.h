/*
 * Copyright (c) 2016-present The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/ZLMediaKit/ZLMediaKit).
 *
 * Use of this source code is governed by MIT-like license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MK_COMMON_H
#define MK_COMMON_H

#include <stdint.h>
#include <stddef.h>

#if defined(GENERATE_EXPORT)
#include "mk_export.h"
#endif

#if defined(_WIN32) && defined(_MSC_VER)
#    define API_CALL __cdecl
#else
#    define API_CALL
#endif

#ifndef _WIN32
#define _strdup strdup
#endif

#if defined(_WIN32) && defined(_MSC_VER)
#    if !defined(GENERATE_EXPORT)
#        if defined(MediaKitApi_EXPORTS)
#            define API_EXPORT __declspec(dllexport)
#        else
#            define API_EXPORT __declspec(dllimport)
#        endif
#    endif
#elif !defined(GENERATE_EXPORT)
#   define API_EXPORT __attribute__((visibility("default")))
#endif

#ifdef __cplusplus
extern "C" {
#endif

// 输出日志到shell  [AUTO-TRANSLATED:6523242b]
// cpp
// Output log to shell
#define LOG_CONSOLE     (1 << 0)
// 输出日志到文件  [AUTO-TRANSLATED:8ffaf1e0]
// Output log to file
#define LOG_FILE        (1 << 1)
// 输出日志到回调函数(mk_events::on_mk_log)  [AUTO-TRANSLATED:616561c1]
// Output log to callback function (mk_events::on_mk_log)
#define LOG_CALLBACK    (1 << 2)

// 向下兼容  [AUTO-TRANSLATED:5b800712]
// Downward compatibility
#define mk_env_init1 mk_env_init2

// 回调user_data回调函数  [AUTO-TRANSLATED:ced626fb]
// Callback user_data callback function
typedef void(API_CALL *on_user_data_free)(void *user_data);

typedef struct {
    // 线程数  [AUTO-TRANSLATED:f7fc7650]
    // Number of threads
    int thread_num;

    // 日志级别,支持0~4  [AUTO-TRANSLATED:f4d77bb5]
    // Log level, supports 0~4
    int log_level;
    // 控制日志输出的掩模，请查看LOG_CONSOLE、LOG_FILE、LOG_CALLBACK等宏  [AUTO-TRANSLATED:71de1d10]
    // Control the mask of log output, please refer to LOG_CONSOLE, LOG_FILE, LOG_CALLBACK macros
    int log_mask;
    // 文件日志保存路径,路径可以不存在(内部可以创建文件夹)，设置为NULL关闭日志输出至文件  [AUTO-TRANSLATED:d0989d3c]
    // File log save path, the path can be non-existent (folders can be created internally), set to NULL to disable log output to file
    const char *log_file_path;
    // 文件日志保存天数,设置为0关闭日志文件  [AUTO-TRANSLATED:04253cb0]
    // File log save days, set to 0 to disable log file
    int log_file_days;

    // 配置文件是内容还是路径  [AUTO-TRANSLATED:b946f030]
    // Is the configuration file content or path
    int ini_is_path;
    // 配置文件内容或路径，可以为NULL,如果该文件不存在，那么将导出默认配置至该文件  [AUTO-TRANSLATED:aeaa4583]
    // Configuration file content or path, can be NULL, if the file does not exist, then the default configuration will be exported to the file
    const char *ini;

    // ssl证书是内容还是路径  [AUTO-TRANSLATED:820671ab]
    // Is the ssl certificate content or path
    int ssl_is_path;
    // ssl证书内容或路径，可以为NULL  [AUTO-TRANSLATED:c32fffb6]
    // ssl certificate content or path, can be NULL
    const char *ssl;
    // 证书密码，可以为NULL  [AUTO-TRANSLATED:b8c9c173]
    // Certificate password, can be NULL
    const char *ssl_pwd;
} mk_config;

/**
 * 初始化环境，调用该库前需要先调用此函数
 * @param cfg 库运行相关参数
 * Initialize the environment, you need to call this function before calling this library
 * @param cfg Library running related parameters
 
 * [AUTO-TRANSLATED:58d6d220]
 */
API_EXPORT void API_CALL mk_env_init(const mk_config *cfg);

/**
 * 关闭所有服务器，请在main函数退出时调用
 * Close all servers, please call this function when exiting the main function
 
 * [AUTO-TRANSLATED:f1148928]
 */
API_EXPORT void API_CALL mk_stop_all_server();

/**
 * 基础类型参数版本的mk_env_init，为了方便其他语言调用
 * @param thread_num 线程数
 * @param log_level 日志级别,支持0~4
 * @param log_mask 日志输出方式掩模，请查看LOG_CONSOLE、LOG_FILE、LOG_CALLBACK等宏
 * @param log_file_path 文件日志保存路径,路径可以不存在(内部可以创建文件夹)，设置为NULL关闭日志输出至文件
 * @param log_file_days 文件日志保存天数,设置为0关闭日志文件
 * @param ini_is_path 配置文件是内容还是路径
 * @param ini 配置文件内容或路径，可以为NULL,如果该文件不存在，那么将导出默认配置至该文件
 * @param ssl_is_path ssl证书是内容还是路径
 * @param ssl ssl证书内容或路径，可以为NULL
 * @param ssl_pwd 证书密码，可以为NULL
 * mk_env_init version of basic type parameters, for easy calling by other languages
 * @param thread_num Number of threads
 * @param log_level Log level, supports 0~4
 * @param log_mask Log output mode mask, please refer to LOG_CONSOLE, LOG_FILE, LOG_CALLBACK macros
 * @param log_file_path File log save path, the path can be non-existent (folders can be created internally), set to NULL to disable log output to file
 * @param log_file_days File log save days, set to 0 to disable log file
 * @param ini_is_path Is the configuration file content or path
 * @param ini Configuration file content or path, can be NULL, if the file does not exist, then the default configuration will be exported to the file
 * @param ssl_is_path Is the ssl certificate content or path
 * @param ssl ssl certificate content or path, can be NULL
 * @param ssl_pwd Certificate password, can be NULL
 
 * [AUTO-TRANSLATED:12901102]
 */
API_EXPORT void API_CALL mk_env_init2(int thread_num,
                                      int log_level,
                                      int log_mask,
                                      const char *log_file_path,
                                      int log_file_days,
                                      int ini_is_path,
                                      const char *ini,
                                      int ssl_is_path,
                                      const char *ssl,
                                      const char *ssl_pwd);

/**
* 设置日志文件
* @param file_max_size 单个切片文件大小(MB)
* @param file_max_count 切片文件个数
 * Set the log file
 * @param file_max_size Single slice file size (MB)
 * @param file_max_count Number of slice files
 
 * [AUTO-TRANSLATED:59204140]
*/
API_EXPORT void API_CALL mk_set_log(int file_max_size, int file_max_count);

/**
 * 设置配置项
 * @deprecated 请使用mk_ini_set_option替代
 * @param key 配置项名
 * @param val 配置项值
 * Set the configuration item
 * @deprecated Please use mk_ini_set_option instead
 * @param key Configuration item name
 * @param val Configuration item value
 
 * [AUTO-TRANSLATED:93d02c07]
 */
API_EXPORT void API_CALL mk_set_option(const char *key, const char *val);

/**
 * 获取配置项的值
 * @deprecated 请使用mk_ini_get_option替代
 * @param key 配置项名
 * Get the value of the configuration item
 * @deprecated Please use mk_ini_get_option instead
 * @param key Configuration item name
 
 * [AUTO-TRANSLATED:6222a231]
 */
API_EXPORT const char * API_CALL mk_get_option(const char *key);


/**
 * 创建http[s]服务器
 * @param port htt监听端口，推荐80，传入0则随机分配
 * @param ssl 是否为ssl类型服务器
 * @return 0:失败,非0:端口号
 * Create http[s] server
 * @param port htt listening port, recommended 80, pass in 0 to randomly allocate
 * @param ssl Whether it is an ssl type server
 * @return 0: failure, non-0: port number
 
 * [AUTO-TRANSLATED:4ca78101]
 */
API_EXPORT uint16_t API_CALL mk_http_server_start(uint16_t port, int ssl);

/**
 * 创建rtsp[s]服务器
 * @param port rtsp监听端口，推荐554，传入0则随机分配
 * @param ssl 是否为ssl类型服务器
 * @return 0:失败,非0:端口号
 * Create rtsp[s] server
 * @param port rtsp listening port, recommended 554, pass in 0 to randomly allocate
 * @param ssl Whether it is an ssl type server
 * @return 0: failure, non-0: port number
 
 * [AUTO-TRANSLATED:3d984d90]
 */
API_EXPORT uint16_t API_CALL mk_rtsp_server_start(uint16_t port, int ssl);

/**
 * 创建rtmp[s]服务器
 * @param port rtmp监听端口，推荐1935，传入0则随机分配
 * @param ssl 是否为ssl类型服务器
 * @return 0:失败,非0:端口号
 * Create rtmp[s] server
 * @param port rtmp listening port, recommended 1935, pass in 0 to randomly allocate
 * @param ssl Whether it is an ssl type server
 * @return 0: failure, non-0: port number
 
 * [AUTO-TRANSLATED:ed841271]
 */
API_EXPORT uint16_t API_CALL mk_rtmp_server_start(uint16_t port, int ssl);

/**
 * 创建rtp服务器
 * @param port rtp监听端口(包括udp/tcp)
 * @return 0:失败,非0:端口号
 * Create rtp server
 * @param port rtp listening port (including udp/tcp)
 * @return 0: failure, non-0: port number
 
 * [AUTO-TRANSLATED:f49af495]
 */
API_EXPORT uint16_t API_CALL mk_rtp_server_start(uint16_t port);

/**
 * 创建rtc服务器
 * @param port rtc监听端口
 * @return 0:失败,非0:端口号
 * Create rtc server
 * @param port rtc listening port
 * @return 0: failure, non-0: port number
 
 * [AUTO-TRANSLATED:df151854]
 */
API_EXPORT uint16_t API_CALL mk_rtc_server_start(uint16_t port);

// 获取webrtc answer sdp回调函数  [AUTO-TRANSLATED:10c93fa9]
// Get webrtc answer sdp callback function
typedef void(API_CALL *on_mk_webrtc_get_answer_sdp)(void *user_data, const char *answer, const char *err);

/**
 * webrtc交换sdp，根据offer sdp生成answer sdp
 * @param user_data 回调用户指针
 * @param cb 回调函数
 * @param type webrtc插件类型，支持echo,play,push
 * @param offer webrtc offer sdp
 * @param url rtc url, 例如 rtc://__defaultVhost/app/stream?key1=val1&key2=val2
 * webrtc exchange sdp, generate answer sdp based on offer sdp
 * @param user_data Callback user pointer
 * @param cb Callback function
 * @param type webrtc plugin type, supports echo, play, push
 * @param offer webrtc offer sdp
 * @param url rtc url, for example rtc://__defaultVhost/app/stream?key1=val1&key2=val2
 
 * [AUTO-TRANSLATED:ea79659b]
 */
API_EXPORT void API_CALL mk_webrtc_get_answer_sdp(void *user_data, on_mk_webrtc_get_answer_sdp cb, const char *type,
                                                  const char *offer, const char *url);

API_EXPORT void API_CALL mk_webrtc_get_answer_sdp2(void *user_data, on_user_data_free user_data_free, on_mk_webrtc_get_answer_sdp cb, const char *type,
                                                  const char *offer, const char *url);

/**
 * 创建srt服务器
 * @param port srt监听端口
 * @return 0:失败,非0:端口号
 * Create srt server
 * @param port srt listening port
 * @return 0: failure, non-0: port number
 
 * [AUTO-TRANSLATED:250984c0]
 */
API_EXPORT uint16_t API_CALL mk_srt_server_start(uint16_t port);


/**
 * 创建shell服务器
 * @param port shell监听端口
 * @return 0:失败,非0:端口号
 * Create shell server
 * @param port shell listening port
 * @return 0: failure, non-0: port number
 
 
 * [AUTO-TRANSLATED:66ec9a2a]
 */
API_EXPORT uint16_t API_CALL mk_shell_server_start(uint16_t port);

#ifdef __cplusplus
}
#endif


#endif /* MK_COMMON_H */
