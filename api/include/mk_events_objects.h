/*
 * Copyright (c) 2016 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/xiongziliang/ZLMediaKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MK_EVENT_OBJECTS_H
#define MK_EVENT_OBJECTS_H
#include "mk_common.h"
#include "mk_tcp.h"
#ifdef __cplusplus
extern "C" {
#endif

///////////////////////////////////////////MP4Info/////////////////////////////////////////////
//MP4Info对象的C映射
typedef void* mk_mp4_info;
//MP4Info::ui64StartedTime
API_EXPORT uint64_t API_CALL mk_mp4_info_get_start_time(const mk_mp4_info ctx);
//MP4Info::ui64TimeLen
API_EXPORT uint64_t API_CALL mk_mp4_info_get_time_len(const mk_mp4_info ctx);
//MP4Info::ui64FileSize
API_EXPORT uint64_t API_CALL mk_mp4_info_get_file_size(const mk_mp4_info ctx);
//MP4Info::strFilePath
API_EXPORT const char* API_CALL mk_mp4_info_get_file_path(const mk_mp4_info ctx);
//MP4Info::strFileName
API_EXPORT const char* API_CALL mk_mp4_info_get_file_name(const mk_mp4_info ctx);
//MP4Info::strFolder
API_EXPORT const char* API_CALL mk_mp4_info_get_folder(const mk_mp4_info ctx);
//MP4Info::strUrl
API_EXPORT const char* API_CALL mk_mp4_info_get_url(const mk_mp4_info ctx);
//MP4Info::strVhost
API_EXPORT const char* API_CALL mk_mp4_info_get_vhost(const mk_mp4_info ctx);
//MP4Info::strAppName
API_EXPORT const char* API_CALL mk_mp4_info_get_app(const mk_mp4_info ctx);
//MP4Info::strStreamId
API_EXPORT const char* API_CALL mk_mp4_info_get_stream(const mk_mp4_info ctx);

///////////////////////////////////////////Parser/////////////////////////////////////////////
//Parser对象的C映射
typedef void* mk_parser;
//Parser::Method(),获取命令字，譬如GET/POST
API_EXPORT const char* API_CALL mk_parser_get_method(const mk_parser ctx);
//Parser::Url(),获取HTTP的访问url(不包括?后面的参数)
API_EXPORT const char* API_CALL mk_parser_get_url(const mk_parser ctx);
//Parser::FullUrl(),包括?后面的参数
API_EXPORT const char* API_CALL mk_parser_get_full_url(const mk_parser ctx);
//Parser::Params(),?后面的参数字符串
API_EXPORT const char* API_CALL mk_parser_get_url_params(const mk_parser ctx);
//Parser::getUrlArgs()["key"],获取?后面的参数中的特定参数
API_EXPORT const char* API_CALL mk_parser_get_url_param(const mk_parser ctx,const char *key);
//Parser::Tail()，获取协议相关信息，譬如 HTTP/1.1
API_EXPORT const char* API_CALL mk_parser_get_tail(const mk_parser ctx);
//Parser::getValues()["key"],获取HTTP头中特定字段
API_EXPORT const char* API_CALL mk_parser_get_header(const mk_parser ctx,const char *key);
//Parser::Content(),获取HTTP body
API_EXPORT const char* API_CALL mk_parser_get_content(const mk_parser ctx, int *length);

///////////////////////////////////////////MediaInfo/////////////////////////////////////////////
//MediaInfo对象的C映射
typedef void* mk_media_info;
//MediaInfo::_param_strs
API_EXPORT const char* API_CALL mk_media_info_get_params(const mk_media_info ctx);
//MediaInfo::_schema
API_EXPORT const char* API_CALL mk_media_info_get_schema(const mk_media_info ctx);
//MediaInfo::_vhost
API_EXPORT const char* API_CALL mk_media_info_get_vhost(const mk_media_info ctx);
//MediaInfo::_app
API_EXPORT const char* API_CALL mk_media_info_get_app(const mk_media_info ctx);
//MediaInfo::_streamid
API_EXPORT const char* API_CALL mk_media_info_get_stream(const mk_media_info ctx);
//MediaInfo::_host
API_EXPORT const char* API_CALL mk_media_info_get_host(const mk_media_info ctx);
//MediaInfo::_port
API_EXPORT uint16_t API_CALL mk_media_info_get_port(const mk_media_info ctx);


///////////////////////////////////////////MediaSource/////////////////////////////////////////////
//MediaSource对象的C映射
typedef void* mk_media_source;
//查找MediaSource的回调函数
typedef void(API_CALL *on_mk_media_source_find_cb)(void *user_data, const mk_media_source ctx);

//MediaSource::getSchema()
API_EXPORT const char* API_CALL mk_media_source_get_schema(const mk_media_source ctx);
//MediaSource::getVhost()
API_EXPORT const char* API_CALL mk_media_source_get_vhost(const mk_media_source ctx);
//MediaSource::getApp()
API_EXPORT const char* API_CALL mk_media_source_get_app(const mk_media_source ctx);
//MediaSource::getId()
API_EXPORT const char* API_CALL mk_media_source_get_stream(const mk_media_source ctx);
//MediaSource::readerCount()
API_EXPORT int API_CALL mk_media_source_get_reader_count(const mk_media_source ctx);
//MediaSource::totalReaderCount()
API_EXPORT int API_CALL mk_media_source_get_total_reader_count(const mk_media_source ctx);
/**
 * 直播源在ZLMediaKit中被称作为MediaSource，
 * 目前支持3种，分别是RtmpMediaSource、RtspMediaSource、HlsMediaSource
 * 源的产生有被动和主动方式:
 * 被动方式分别是rtsp/rtmp/rtp推流、mp4点播
 * 主动方式包括mk_media_create创建的对象(DevChannel)、mk_proxy_player_create创建的对象(PlayerProxy)
 * 被动方式你不用做任何处理，ZLMediaKit已经默认适配了MediaSource::close()事件，都会关闭直播流
 * 主动方式你要设置这个事件的回调，你要自己选择删除对象
 * 通过mk_proxy_player_set_on_close、mk_media_set_on_close函数可以设置回调,
 * 请在回调中删除对象来完成媒体的关闭，否则又为什么要调用mk_media_source_close函数？
 * @param ctx 对象
 * @param force 是否强制关闭，如果强制关闭，在有人观看的情况下也会关闭
 * @return 0代表失败，1代表成功
 */
API_EXPORT int API_CALL mk_media_source_close(const mk_media_source ctx,int force);
//MediaSource::seekTo()
API_EXPORT int API_CALL mk_media_source_seek_to(const mk_media_source ctx,uint32_t stamp);

//MediaSource::find()
API_EXPORT void API_CALL mk_media_source_find(const char *schema,
                                              const char *vhost,
                                              const char *app,
                                              const char *stream,
                                              void *user_data,
                                              on_mk_media_source_find_cb cb);
//MediaSource::for_each_media()
API_EXPORT void API_CALL mk_media_source_for_each(void *user_data, on_mk_media_source_find_cb cb);

///////////////////////////////////////////HttpBody/////////////////////////////////////////////
//HttpBody对象的C映射
typedef void* mk_http_body;
/**
 * 生成HttpStringBody
 * @param str 字符串指针
 * @param len 字符串长度，为0则用strlen获取
 */
API_EXPORT mk_http_body API_CALL mk_http_body_from_string(const char *str,int len);

/**
 * 生成HttpFileBody
 * @param file_path 文件完整路径
 */
API_EXPORT mk_http_body API_CALL mk_http_body_from_file(const char *file_path);

/**
 * 生成HttpMultiFormBody
 * @param key_val 参数key-value
 * @param file_path 文件完整路径
 */
API_EXPORT mk_http_body API_CALL mk_http_body_from_multi_form(const char *key_val[],const char *file_path);

/**
 * 销毁HttpBody
 */
API_EXPORT void API_CALL mk_http_body_release(mk_http_body ctx);

///////////////////////////////////////////HttpResponseInvoker/////////////////////////////////////////////
//HttpSession::HttpResponseInvoker对象的C映射
typedef void* mk_http_response_invoker;

/**
 * HttpSession::HttpResponseInvoker(const string &codeOut, const StrCaseMap &headerOut, const HttpBody::Ptr &body);
 * @param response_code 譬如200 OK
 * @param response_header 返回的http头，譬如 {"Content-Type","text/html",NULL} 必须以NULL结尾
 * @param response_body body对象
 */
API_EXPORT void API_CALL mk_http_response_invoker_do(const mk_http_response_invoker ctx,
                                                     const char *response_code,
                                                     const char **response_header,
                                                     const mk_http_body response_body);

/**
 * HttpSession::HttpResponseInvoker(const string &codeOut, const StrCaseMap &headerOut, const string &body);
 * @param response_code 譬如200 OK
 * @param response_header 返回的http头，譬如 {"Content-Type","text/html",NULL} 必须以NULL结尾
 * @param response_content 返回的content部分，譬如一个网页内容
 */
API_EXPORT void API_CALL mk_http_response_invoker_do_string(const mk_http_response_invoker ctx,
                                                            const char *response_code,
                                                            const char **response_header,
                                                            const char *response_content);
/**
 * HttpSession::HttpResponseInvoker(const StrCaseMap &requestHeader,const StrCaseMap &responseHeader,const string &filePath);
 * @param request_parser 请求事件中的mk_parser对象，用于提取其中http头中的Range字段，通过该字段先fseek然后再发送文件部分片段
 * @param response_header 返回的http头，譬如 {"Content-Type","text/html",NULL} 必须以NULL结尾
 * @param response_file_path 返回的content部分，譬如/path/to/html/file
 */
API_EXPORT void API_CALL mk_http_response_invoker_do_file(const mk_http_response_invoker ctx,
                                                          const mk_parser request_parser,
                                                          const char *response_header[],
                                                          const char *response_file_path);
/**
* 克隆mk_http_response_invoker对象，通过克隆对象为堆对象，可以实现跨线程异步执行mk_http_response_invoker_do
* 如果是同步执行mk_http_response_invoker_do，那么没必要克隆对象
*/
API_EXPORT mk_http_response_invoker API_CALL mk_http_response_invoker_clone(const mk_http_response_invoker ctx);

/**
 * 销毁堆上的克隆对象
 */
API_EXPORT void API_CALL mk_http_response_invoker_clone_release(const mk_http_response_invoker ctx);

///////////////////////////////////////////HttpAccessPathInvoker/////////////////////////////////////////////
//HttpSession::HttpAccessPathInvoker对象的C映射
typedef void* mk_http_access_path_invoker;
/**
 * HttpSession::HttpAccessPathInvoker(const string &errMsg,const string &accessPath, int cookieLifeSecond);
 * @param err_msg 如果为空，则代表鉴权通过，否则为错误提示,可以为null
 * @param access_path 运行或禁止访问的根目录,可以为null
 * @param cookie_life_second 鉴权cookie有效期
 **/
API_EXPORT void API_CALL mk_http_access_path_invoker_do(const mk_http_access_path_invoker ctx,
                                                        const char *err_msg,
                                                        const char *access_path,
                                                        int cookie_life_second);

/**
* 克隆mk_http_access_path_invoker对象，通过克隆对象为堆对象，可以实现跨线程异步执行mk_http_access_path_invoker_do
* 如果是同步执行mk_http_access_path_invoker_do，那么没必要克隆对象
*/
API_EXPORT mk_http_access_path_invoker API_CALL mk_http_access_path_invoker_clone(const mk_http_access_path_invoker ctx);

/**
 * 销毁堆上的克隆对象
 */
API_EXPORT void API_CALL mk_http_access_path_invoker_clone_release(const mk_http_access_path_invoker ctx);

///////////////////////////////////////////RtspSession::onGetRealm/////////////////////////////////////////////
//RtspSession::onGetRealm对象的C映射
typedef void* mk_rtsp_get_realm_invoker;
/**
 * 执行RtspSession::onGetRealm
 * @param realm 该rtsp流是否需要开启rtsp专属鉴权，至null或空字符串则不鉴权
 */
API_EXPORT void API_CALL mk_rtsp_get_realm_invoker_do(const mk_rtsp_get_realm_invoker ctx,
                                                      const char *realm);

/**
* 克隆mk_rtsp_get_realm_invoker对象，通过克隆对象为堆对象，可以实现跨线程异步执行mk_rtsp_get_realm_invoker_do
* 如果是同步执行mk_rtsp_get_realm_invoker_do，那么没必要克隆对象
*/
API_EXPORT mk_rtsp_get_realm_invoker API_CALL mk_rtsp_get_realm_invoker_clone(const mk_rtsp_get_realm_invoker ctx);

/**
 * 销毁堆上的克隆对象
 */
API_EXPORT void API_CALL mk_rtsp_get_realm_invoker_clone_release(const mk_rtsp_get_realm_invoker ctx);

///////////////////////////////////////////RtspSession::onAuth/////////////////////////////////////////////
//RtspSession::onAuth对象的C映射
typedef void* mk_rtsp_auth_invoker;

/**
 * 执行RtspSession::onAuth
 * @param encrypted 为true是则表明是md5加密的密码，否则是明文密码, 在请求明文密码时如果提供md5密码者则会导致认证失败
 * @param pwd_or_md5 明文密码或者md5加密的密码
 */
API_EXPORT void API_CALL mk_rtsp_auth_invoker_do(const mk_rtsp_auth_invoker ctx,
                                                 int encrypted,
                                                 const char *pwd_or_md5);

/**
 * 克隆mk_rtsp_auth_invoker对象，通过克隆对象为堆对象，可以实现跨线程异步执行mk_rtsp_auth_invoker_do
 * 如果是同步执行mk_rtsp_auth_invoker_do，那么没必要克隆对象
 */
API_EXPORT mk_rtsp_auth_invoker API_CALL mk_rtsp_auth_invoker_clone(const mk_rtsp_auth_invoker ctx);

/**
 * 销毁堆上的克隆对象
 */
API_EXPORT void API_CALL mk_rtsp_auth_invoker_clone_release(const mk_rtsp_auth_invoker ctx);

///////////////////////////////////////////Broadcast::PublishAuthInvoker/////////////////////////////////////////////
//Broadcast::PublishAuthInvoker对象的C映射
typedef void* mk_publish_auth_invoker;

/**
 * 执行Broadcast::PublishAuthInvoker
 * @param err_msg 为空或null则代表鉴权成功
 * @param enable_rtxp rtmp推流时是否运行转rtsp；rtsp推流时，是否允许转rtmp
 * @param enable_hls 是否允许转换hls
 * @param enable_mp4 是否运行MP4录制
 */
API_EXPORT void API_CALL mk_publish_auth_invoker_do(const mk_publish_auth_invoker ctx,
                                                    const char *err_msg,
                                                    int enable_rtxp,
                                                    int enable_hls,
                                                    int enable_mp4);

/**
 * 克隆mk_publish_auth_invoker对象，通过克隆对象为堆对象，可以实现跨线程异步执行mk_publish_auth_invoker_do
 * 如果是同步执行mk_publish_auth_invoker_do，那么没必要克隆对象
 */
API_EXPORT mk_publish_auth_invoker API_CALL mk_publish_auth_invoker_clone(const mk_publish_auth_invoker ctx);

/**
 * 销毁堆上的克隆对象
 */
API_EXPORT void API_CALL mk_publish_auth_invoker_clone_release(const mk_publish_auth_invoker ctx);

///////////////////////////////////////////Broadcast::AuthInvoker/////////////////////////////////////////////
//Broadcast::AuthInvoker对象的C映射
typedef void* mk_auth_invoker;

/**
 * 执行Broadcast::AuthInvoker
 * @param err_msg 为空或null则代表鉴权成功
 */
API_EXPORT void API_CALL mk_auth_invoker_do(const mk_auth_invoker ctx, const char *err_msg);

/**
 * 克隆mk_auth_invoker对象，通过克隆对象为堆对象，可以实现跨线程异步执行mk_auth_invoker_do
 * 如果是同步执行mk_auth_invoker_do，那么没必要克隆对象
 */
API_EXPORT mk_auth_invoker API_CALL mk_auth_invoker_clone(const mk_auth_invoker ctx);

/**
 * 销毁堆上的克隆对象
 */
API_EXPORT void API_CALL mk_auth_invoker_clone_release(const mk_auth_invoker ctx);

#ifdef __cplusplus
}
#endif
#endif //MK_EVENT_OBJECTS_H
