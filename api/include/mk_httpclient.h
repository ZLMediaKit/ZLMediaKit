/*
 * Copyright (c) 2016-present The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/ZLMediaKit/ZLMediaKit).
 *
 * Use of this source code is governed by MIT-like license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MK_HTTPCLIENT_H_
#define MK_HTTPCLIENT_H_

#include "mk_common.h"
#include "mk_events_objects.h"

#ifdef __cplusplus
extern "C" {
#endif

///////////////////////////////////////////HttpDownloader/////////////////////////////////////////////

typedef struct mk_http_downloader_t *mk_http_downloader;

/**
 * @param user_data 用户数据指针
 * @param code 错误代码，0代表成功
 * @param err_msg 错误提示
 * @param file_path 文件保存路径
 * @param user_data User data pointer
 * @param code Error code, 0 represents success
 * @param err_msg Error message
 * @param file_path File save path
 
 * [AUTO-TRANSLATED:8f8ed7ef]
 */
typedef void(API_CALL *on_mk_download_complete)(void *user_data, int code, const char *err_msg, const char *file_path);

/**
 * 创建http[s]下载器
 * @return 下载器指针
 * Create http[s] downloader
 * @return Downloader pointer
 
 * [AUTO-TRANSLATED:93112194]
 */
API_EXPORT mk_http_downloader API_CALL mk_http_downloader_create();

/**
 * 销毁http[s]下载器
 * @param ctx 下载器指针
 * Destroy http[s] downloader
 * @param ctx Downloader pointer
 
 * [AUTO-TRANSLATED:8378a5a7]
 */
API_EXPORT void API_CALL mk_http_downloader_release(mk_http_downloader ctx);

/**
 * 开始http[s]下载
 * @param ctx 下载器指针
 * @param url http[s]下载url
 * @param file 文件保存路径
 * @param cb 回调函数
 * @param user_data 用户数据指针
 * Start http[s] download
 * @param ctx Downloader pointer
 * @param url http[s] download url
 * @param file File save path
 * @param cb Callback function
 * @param user_data User data pointer
 
 * [AUTO-TRANSLATED:8a2acf02]
 */
API_EXPORT void API_CALL mk_http_downloader_start(mk_http_downloader ctx, const char *url, const char *file, on_mk_download_complete cb, void *user_data);
API_EXPORT void API_CALL mk_http_downloader_start2(mk_http_downloader ctx, const char *url, const char *file, on_mk_download_complete cb, void *user_data, on_user_data_free user_data_free);

///////////////////////////////////////////HttpRequester/////////////////////////////////////////////
typedef struct mk_http_requester_t *mk_http_requester;

/**
 * http请求结果回调
 * 在code == 0时代表本次http会话是完整的（收到了http回复）
 * 用户应该通过user_data获取到mk_http_requester对象
 * 然后通过mk_http_requester_get_response等函数获取相关回复数据
 * 在回调结束时，应该通过mk_http_requester_release函数销毁该对象
 * 或者调用mk_http_requester_clear函数后再复用该对象
 * @param user_data 用户数据指针
 * @param code 错误代码，0代表成功
 * @param err_msg 错误提示
 * Http request result callback
 * When code == 0, it means that the current http session is complete (http response has been received)
 * Users should get the mk_http_requester object through user_data
 * Then get the relevant response data through functions such as mk_http_requester_get_response
 * At the end of the callback, the object should be destroyed by calling the mk_http_requester_release function
 * Or reuse the object after calling the mk_http_requester_clear function
 * @param user_data User data pointer
 * @param code Error code, 0 represents success
 * @param err_msg Error message
 
 * [AUTO-TRANSLATED:d24408ce]
 */
typedef void(API_CALL *on_mk_http_requester_complete)(void *user_data, int code, const char *err_msg);

/**
 * 创建HttpRequester
 * Create HttpRequester
 
 * [AUTO-TRANSLATED:fa182fbc]
 */
API_EXPORT mk_http_requester API_CALL mk_http_requester_create();

/**
 * 在复用mk_http_requester对象时才需要用到此方法
 * This method is only needed when reusing the mk_http_requester object
 
 * [AUTO-TRANSLATED:6854d97f]
 */
API_EXPORT void API_CALL mk_http_requester_clear(mk_http_requester ctx);

/**
 * 销毁HttpRequester
 * 如果调用了mk_http_requester_start函数且正在等待http回复，
 * 也可以调用mk_http_requester_release方法取消本次http请求
 * Destroy HttpRequester
 * If the mk_http_requester_start function is called and is waiting for the http response,
 * You can also call the mk_http_requester_release method to cancel the current http request
 
 * [AUTO-TRANSLATED:5f533e28]
 */
API_EXPORT void API_CALL mk_http_requester_release(mk_http_requester ctx);

/**
 * 设置HTTP方法，譬如GET/POST
 * Set HTTP method, such as GET/POST
 
 * [AUTO-TRANSLATED:d4b641f1]
 */
API_EXPORT void API_CALL mk_http_requester_set_method(mk_http_requester ctx,const char *method);

/**
 * 批量设置设置HTTP头
 * @param header 譬如 {"Content-Type","text/html",NULL} 必须以NULL结尾
 * Batch set HTTP headers
 * @param header For example, {"Content-Type","text/html",NULL} must end with NULL
 
 * [AUTO-TRANSLATED:65124347]
 */
API_EXPORT void API_CALL mk_http_requester_set_header(mk_http_requester ctx, const char *header[]);

/**
 * 添加HTTP头
 * @param key 譬如Content-Type
 * @param value 譬如 text/html
 * @param force 如果已经存在该key，是否强制替换
 * Add HTTP header
 * @param key For example, Content-Type
 * @param value For example, text/html
 * @param force If the key already exists, whether to force replacement
 
 * [AUTO-TRANSLATED:79d32682]
 */
API_EXPORT void API_CALL mk_http_requester_add_header(mk_http_requester ctx,const char *key,const char *value,int force);

/**
 * 设置消息体，
 * @param body mk_http_body对象，通过mk_http_body_from_string等函数生成，使用完毕后请调用mk_http_body_release释放之
 * Set message body,
 * @param body mk_http_body object, generated by functions such as mk_http_body_from_string, please call mk_http_body_release to release it after use
 
 * [AUTO-TRANSLATED:85d0f139]
 */
API_EXPORT void API_CALL mk_http_requester_set_body(mk_http_requester ctx, mk_http_body body);

/**
 * 在收到HTTP回复后可调用该方法获取状态码
 * @return 譬如 200 OK
 * You can call this method to get the status code after receiving the HTTP response
 * @return For example, 200 OK
 
 * [AUTO-TRANSLATED:7757b21a]
 */
API_EXPORT const char* API_CALL mk_http_requester_get_response_status(mk_http_requester ctx);

/**
 * 在收到HTTP回复后可调用该方法获取响应HTTP头
 * @param key HTTP头键名
 * @return  HTTP头键值
 * You can call this method to get the response HTTP header after receiving the HTTP response
 * @param key HTTP header key name
 * @return HTTP header key value
 
 * [AUTO-TRANSLATED:10f8ae74]
 */
API_EXPORT const char* API_CALL mk_http_requester_get_response_header(mk_http_requester ctx,const char *key);

/**
 * 在收到HTTP回复后可调用该方法获取响应HTTP body
 * @param length 返回body长度,可以为null
 * @return body指针
 * You can call this method to get the response HTTP body after receiving the HTTP response
 * @param length Return body length, can be null
 * @return Body pointer
 
 * [AUTO-TRANSLATED:764dbb38]
 */
API_EXPORT const char* API_CALL mk_http_requester_get_response_body(mk_http_requester ctx, size_t *length);

/**
 * 在收到HTTP回复后可调用该方法获取响应
 * @return 响应对象
 * You can call this method to get the response after receiving the HTTP response
 * @return Response object
 
 * [AUTO-TRANSLATED:3800b175]
 */
API_EXPORT mk_parser API_CALL mk_http_requester_get_response(mk_http_requester ctx);

/**
 * 设置回调函数
 * @param cb 回调函数，不能为空
 * @param user_data 用户数据指针
 * Set callback function
 * @param cb Callback function, cannot be empty
 * @param user_data User data pointer
 
 * [AUTO-TRANSLATED:f04412b8]
 */
API_EXPORT void API_CALL mk_http_requester_set_cb(mk_http_requester ctx,on_mk_http_requester_complete cb, void *user_data);
API_EXPORT void API_CALL mk_http_requester_set_cb2(mk_http_requester ctx,on_mk_http_requester_complete cb, void *user_data, on_user_data_free user_data_free);

/**
 * 开始url请求
 * @param url 请求url，支持http/https
 * @param timeout_second 最大超时时间
 * Start url request
 * @param url Request url, supports http/https
 * @param timeout_second Maximum timeout time
 
 * [AUTO-TRANSLATED:36986fec]
 */
API_EXPORT void API_CALL mk_http_requester_start(mk_http_requester ctx,const char *url, float timeout_second);

#ifdef __cplusplus
}
#endif

#endif /* MK_HTTPCLIENT_H_ */
