/*
 * Copyright (c) 2016 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/xiongziliang/ZLMediaKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
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

typedef void *mk_http_downloader;

/**
 * @param user_data 用户数据指针
 * @param code 错误代码，0代表成功
 * @param err_msg 错误提示
 * @param file_path 文件保存路径
 */
typedef void(API_CALL *on_mk_download_complete)(void *user_data, int code, const char *err_msg, const char *file_path);

/**
 * 创建http[s]下载器
 * @return 下载器指针
 */
API_EXPORT mk_http_downloader API_CALL mk_http_downloader_create();

/**
 * 销毁http[s]下载器
 * @param ctx 下载器指针
 */
API_EXPORT void API_CALL mk_http_downloader_release(mk_http_downloader ctx);

/**
 * 开始http[s]下载
 * @param ctx 下载器指针
 * @param url http[s]下载url
 * @param file 文件保存路径
 * @param cb 回调函数
 * @param user_data 用户数据指针
 */
API_EXPORT void API_CALL mk_http_downloader_start(mk_http_downloader ctx, const char *url, const char *file, on_mk_download_complete cb, void *user_data);


///////////////////////////////////////////HttpRequester/////////////////////////////////////////////
typedef void *mk_http_requester;

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
 */
typedef void(API_CALL *on_mk_http_requester_complete)(void *user_data, int code, const char *err_msg);

/**
 * 创建HttpRequester
 */
API_EXPORT mk_http_requester API_CALL mk_http_requester_create();

/**
 * 在复用mk_http_requester对象时才需要用到此方法
 */
API_EXPORT void API_CALL mk_http_requester_clear(mk_http_requester ctx);

/**
 * 销毁HttpRequester
 * 如果调用了mk_http_requester_start函数且正在等待http回复，
 * 也可以调用mk_http_requester_release方法取消本次http请求
 */
API_EXPORT void API_CALL mk_http_requester_release(mk_http_requester ctx);

/**
 * 设置HTTP方法，譬如GET/POST
 */
API_EXPORT void API_CALL mk_http_requester_set_method(mk_http_requester ctx,const char *method);

/**
 * 批量设置设置HTTP头
 * @param header 譬如 {"Content-Type","text/html",NULL} 必须以NULL结尾
 */
API_EXPORT void API_CALL mk_http_requester_set_header(mk_http_requester ctx, const char *header[]);

/**
 * 添加HTTP头
 * @param key 譬如Content-Type
 * @param value 譬如 text/html
 * @param force 如果已经存在该key，是否强制替换
 */
API_EXPORT void API_CALL mk_http_requester_add_header(mk_http_requester ctx,const char *key,const char *value,int force);

/**
 * 设置消息体，
 * @param body mk_http_body对象，通过mk_http_body_from_string等函数生成，使用完毕后请调用mk_http_body_release释放之
 */
API_EXPORT void API_CALL mk_http_requester_set_body(mk_http_requester ctx, mk_http_body body);

/**
 * 在收到HTTP回复后可调用该方法获取状态码
 * @return 譬如 200 OK
 */
API_EXPORT const char* API_CALL mk_http_requester_get_response_status(mk_http_requester ctx);

/**
 * 在收到HTTP回复后可调用该方法获取响应HTTP头
 * @param key HTTP头键名
 * @return  HTTP头键值
 */
API_EXPORT const char* API_CALL mk_http_requester_get_response_header(mk_http_requester ctx,const char *key);

/**
 * 在收到HTTP回复后可调用该方法获取响应HTTP body
 * @param length 返回body长度,可以为null
 * @return body指针
 */
API_EXPORT const char* API_CALL mk_http_requester_get_response_body(mk_http_requester ctx, int *length);

/**
 * 在收到HTTP回复后可调用该方法获取响应
 * @return 响应对象
 */
API_EXPORT mk_parser API_CALL mk_http_requester_get_response(mk_http_requester ctx);

/**
 * 设置回调函数
 * @param cb 回调函数，不能为空
 * @param user_data 用户数据指针
 */
API_EXPORT void API_CALL mk_http_requester_set_cb(mk_http_requester ctx,on_mk_http_requester_complete cb, void *user_data);

/**
 * 开始url请求
 * @param url 请求url，支持http/https
 * @param timeout_second 最大超时时间
 */
API_EXPORT void API_CALL mk_http_requester_start(mk_http_requester ctx,const char *url, float timeout_second);

#ifdef __cplusplus
}
#endif

#endif /* MK_HTTPCLIENT_H_ */
