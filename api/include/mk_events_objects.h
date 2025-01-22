/*
 * Copyright (c) 2016-present The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/ZLMediaKit/ZLMediaKit).
 *
 * Use of this source code is governed by MIT-like license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MK_EVENT_OBJECTS_H
#define MK_EVENT_OBJECTS_H
#include "mk_common.h"
#include "mk_tcp.h"
#include "mk_track.h"
#include "mk_util.h"
#ifdef __cplusplus
extern "C" {
#endif

///////////////////////////////////////////RecordInfo/////////////////////////////////////////////
// RecordInfo对象的C映射  [AUTO-TRANSLATED:2c7825a6]
// RecordInfo object's C mapping
typedef struct mk_record_info_t *mk_record_info;
// GMT 标准时间，单位秒  [AUTO-TRANSLATED:3b827274]
// GMT standard time, unit is seconds
API_EXPORT uint64_t API_CALL mk_record_info_get_start_time(const mk_record_info ctx);
// 录像长度，单位秒  [AUTO-TRANSLATED:1dceac0c]
// Recording length, unit is seconds
API_EXPORT float API_CALL mk_record_info_get_time_len(const mk_record_info ctx);
// 文件大小，单位 BYTE  [AUTO-TRANSLATED:add20c50]
// File size, unit is BYTE
API_EXPORT size_t API_CALL mk_record_info_get_file_size(const mk_record_info ctx);
// 文件路径  [AUTO-TRANSLATED:c5246c5d]
// File path
API_EXPORT const char *API_CALL mk_record_info_get_file_path(const mk_record_info ctx);
// 文件名称  [AUTO-TRANSLATED:b5d7c753]
// File name
API_EXPORT const char *API_CALL mk_record_info_get_file_name(const mk_record_info ctx);
// 文件夹路径  [AUTO-TRANSLATED:0e5c9d26]
// Folder path
API_EXPORT const char *API_CALL mk_record_info_get_folder(const mk_record_info ctx);
// 播放路径  [AUTO-TRANSLATED:9b5c99f8]
// Playback path
API_EXPORT const char *API_CALL mk_record_info_get_url(const mk_record_info ctx);
// 应用名称  [AUTO-TRANSLATED:2aa47ea2]
// Application name
API_EXPORT const char *API_CALL mk_record_info_get_vhost(const mk_record_info ctx);
// 流 ID  [AUTO-TRANSLATED:4bbe1cbe]
// Stream ID
API_EXPORT const char *API_CALL mk_record_info_get_app(const mk_record_info ctx);
// 虚拟主机  [AUTO-TRANSLATED:aaae9cfe]
// Virtual host
API_EXPORT const char *API_CALL mk_record_info_get_stream(const mk_record_info ctx);

// // 下面宏保障用户代码兼容性, 二进制abi不兼容，用户需要重新编译链接 /////  [AUTO-TRANSLATED:e532a596]
// // The following macros ensure user code compatibility, binary abi is incompatible, users need to recompile and link /////
#define mk_mp4_info mk_record_info
#define mk_mp4_info_get_start_time mk_record_info_get_start_time
#define mk_mp4_info_get_time_len mk_record_info_get_time_len
#define mk_mp4_info_get_file_size mk_record_info_get_file_size
#define mk_mp4_info_get_file_path mk_record_info_get_file_path
#define mk_mp4_info_get_file_name mk_record_info_get_file_name
#define mk_mp4_info_get_folder mk_record_info_get_folder
#define mk_mp4_info_get_url mk_record_info_get_url
#define mk_mp4_info_get_vhost mk_record_info_get_vhost
#define mk_mp4_info_get_app mk_record_info_get_app
#define mk_mp4_info_get_stream mk_record_info_get_stream

///////////////////////////////////////////Parser/////////////////////////////////////////////
// Parser对象的C映射  [AUTO-TRANSLATED:6ceb91d6]
// Parser object's C mapping
typedef struct mk_parser_t *mk_parser;
// Parser对象中Headers foreach回调  [AUTO-TRANSLATED:4e0a6646]
// Parser object's Headers foreach callback
typedef void(API_CALL *on_mk_parser_header_cb)(void *user_data, const char *key, const char *val);
// Parser::Method(),获取命令字，譬如GET/POST  [AUTO-TRANSLATED:904ebe57]
// Parser::Method(), get the command word, such as GET/POST
API_EXPORT const char* API_CALL mk_parser_get_method(const mk_parser ctx);
// Parser::Url(),获取HTTP的访问url(不包括?后面的参数)  [AUTO-TRANSLATED:75f320c5]
// Parser::Url(), get the HTTP access url (excluding the parameters after ?)
API_EXPORT const char* API_CALL mk_parser_get_url(const mk_parser ctx);
// Parser::Params(),?后面的参数字符串  [AUTO-TRANSLATED:3745fec0]
// Parser::Params(), the parameter string after ?
API_EXPORT const char* API_CALL mk_parser_get_url_params(const mk_parser ctx);
// Parser::getUrlArgs()["key"],获取?后面的参数中的特定参数  [AUTO-TRANSLATED:425e4b61]
// Parser::getUrlArgs()["key"], get the specific parameter in the parameters after ?
API_EXPORT const char* API_CALL mk_parser_get_url_param(const mk_parser ctx,const char *key);
// Parser::Tail()，获取协议相关信息，譬如 HTTP/1.1  [AUTO-TRANSLATED:786534b6]
// Parser::Tail(), get protocol related information, such as HTTP/1.1
API_EXPORT const char* API_CALL mk_parser_get_tail(const mk_parser ctx);
// Parser::getValues()["key"],获取HTTP头中特定字段  [AUTO-TRANSLATED:1c210637]
// Parser::getValues()["key"], get the specific field in the HTTP header
API_EXPORT const char* API_CALL mk_parser_get_header(const mk_parser ctx,const char *key);
// Parser::Content(),获取HTTP body  [AUTO-TRANSLATED:fb05b253]
// Parser::Content(), get the HTTP body
API_EXPORT const char* API_CALL mk_parser_get_content(const mk_parser ctx, size_t *length);
// 循环获取所有header  [AUTO-TRANSLATED:9fd7571a]
// Loop to get all headers
API_EXPORT void API_CALL mk_parser_headers_for_each(const mk_parser ctx, on_mk_parser_header_cb cb, void *user_data);

///////////////////////////////////////////MediaInfo/////////////////////////////////////////////
// MediaInfo对象的C映射  [AUTO-TRANSLATED:f9649086]
// MediaInfo object's C mapping
typedef struct mk_media_info_t *mk_media_info;
//MediaInfo::param_strs
API_EXPORT const char* API_CALL mk_media_info_get_params(const mk_media_info ctx);
//MediaInfo::schema
API_EXPORT const char* API_CALL mk_media_info_get_schema(const mk_media_info ctx);
//MediaInfo::vhost
API_EXPORT const char* API_CALL mk_media_info_get_vhost(const mk_media_info ctx);
//MediaInfo::app
API_EXPORT const char* API_CALL mk_media_info_get_app(const mk_media_info ctx);
//MediaInfo::stream
API_EXPORT const char* API_CALL mk_media_info_get_stream(const mk_media_info ctx);
//MediaInfo::host
API_EXPORT const char* API_CALL mk_media_info_get_host(const mk_media_info ctx);
//MediaInfo::port
API_EXPORT uint16_t API_CALL mk_media_info_get_port(const mk_media_info ctx);


///////////////////////////////////////////MediaSource/////////////////////////////////////////////
// MediaSource对象的C映射  [AUTO-TRANSLATED:feb50a09]
// MediaSource object's C mapping
typedef struct mk_media_source_t *mk_media_source;
// 查找MediaSource的回调函数  [AUTO-TRANSLATED:e8b54cf9]
// Callback function to find MediaSource
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
// get track count from MediaSource
API_EXPORT int API_CALL mk_media_source_get_track_count(const mk_media_source ctx);
// copy track reference by index from MediaSource, please use mk_track_unref to release it
API_EXPORT mk_track API_CALL mk_media_source_get_track(const mk_media_source ctx, int index);
// MediaSource::Track:loss
API_EXPORT float API_CALL mk_media_source_get_track_loss(const mk_media_source ctx, const mk_track track);
// MediaSource::broadcastMessage
API_EXPORT int API_CALL mk_media_source_broadcast_msg(const mk_media_source ctx, const char *msg, size_t len);
// MediaSource::getOriginUrl()
API_EXPORT const char* API_CALL mk_media_source_get_origin_url(const mk_media_source ctx);
// MediaSource::getOriginType()
API_EXPORT int API_CALL mk_media_source_get_origin_type(const mk_media_source ctx);
// MediaSource::getOriginTypeStr(), 使用后请用mk_free释放返回值  [AUTO-TRANSLATED:d612ec22]
// MediaSource::getOriginTypeStr(), please use mk_free to release the return value after use
API_EXPORT const char *API_CALL mk_media_source_get_origin_type_str(const mk_media_source ctx);
// MediaSource::getCreateStamp()
API_EXPORT uint64_t API_CALL mk_media_source_get_create_stamp(const mk_media_source ctx);
// MediaSource::isRecording()  0:hls,1:MP4
API_EXPORT int API_CALL mk_media_source_is_recording(const mk_media_source ctx, int type);
// MediaSource::getBytesSpeed() 
API_EXPORT int API_CALL mk_media_source_get_bytes_speed(const mk_media_source ctx);
// MediaSource::getAliveSecond()
API_EXPORT uint64_t API_CALL mk_media_source_get_alive_second(const mk_media_source ctx);
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
 * Live sources are called MediaSource in ZLMediaKit,
 * Currently, there are 3 types, namely RtmpMediaSource, RtspMediaSource, HlsMediaSource
 * The source is generated in both passive and active ways:
 * Passive ways are rtsp/rtmp/rtp push stream, mp4 on-demand
 * Active ways include objects created by mk_media_create (DevChannel), objects created by mk_proxy_player_create (PlayerProxy)
 * You don't need to do anything for passive ways, ZLMediaKit has already adapted the MediaSource::close() event by default, which will close the live stream
 * For active ways, you need to set the callback of this event, you need to choose to delete the object yourself
 * You can set the callback through the mk_proxy_player_set_on_close and mk_media_set_on_close functions,
 * Please delete the object in the callback to complete the media closure, otherwise why call the mk_media_source_close function?
 * @param ctx object
 * @param force Whether to force closure, if forced closure, it will be closed even if someone is watching
 * @return 0 means failure, 1 means success
 
 * [AUTO-TRANSLATED:9415a405]
 */
API_EXPORT int API_CALL mk_media_source_close(const mk_media_source ctx,int force);
//MediaSource::seekTo()
API_EXPORT int API_CALL mk_media_source_seek_to(const mk_media_source ctx,uint32_t stamp);

/**
 * rtp推流成功与否的回调(第一次成功后，后面将一直重试)
 * Callback for whether rtp push stream is successful or not (after the first success, it will keep retrying)
 
 * [AUTO-TRANSLATED:7e00f7fb]
 */
typedef void(API_CALL *on_mk_media_source_send_rtp_result)(void *user_data, uint16_t local_port, int err, const char *msg);

// MediaSource::startSendRtp,请参考mk_media_start_send_rtp,注意ctx参数类型不一样  [AUTO-TRANSLATED:515ab2e3]
// MediaSource::startSendRtp, please refer to mk_media_start_send_rtp, note that the ctx parameter type is different
API_EXPORT void API_CALL mk_media_source_start_send_rtp(const mk_media_source ctx, const char *dst_url, uint16_t dst_port, const char *ssrc, int con_type, on_mk_media_source_send_rtp_result cb, void *user_data);
API_EXPORT void API_CALL mk_media_source_start_send_rtp2(const mk_media_source ctx, const char *dst_url, uint16_t dst_port, const char *ssrc, int con_type, on_mk_media_source_send_rtp_result cb, void *user_data, on_user_data_free user_data_free);
API_EXPORT void API_CALL mk_media_source_start_send_rtp3(const mk_media_source ctx, const char *dst_url, uint16_t dst_port, const char *ssrc, int con_type, mk_ini options, on_mk_media_source_send_rtp_result cb, void *user_data);
API_EXPORT void API_CALL mk_media_source_start_send_rtp4(const mk_media_source ctx, const char *dst_url, uint16_t dst_port, const char *ssrc, int con_type, mk_ini options, on_mk_media_source_send_rtp_result cb,void *user_data, on_user_data_free user_data_free);
// MediaSource::stopSendRtp，请参考mk_media_stop_send_rtp,注意ctx参数类型不一样  [AUTO-TRANSLATED:415fe454]
// MediaSource::stopSendRtp, please refer to mk_media_stop_send_rtp, note that the ctx parameter type is different
API_EXPORT int API_CALL mk_media_source_stop_send_rtp(const mk_media_source ctx);

//MediaSource::find()
API_EXPORT void API_CALL mk_media_source_find(const char *schema,
                                              const char *vhost,
                                              const char *app,
                                              const char *stream,
                                              int from_mp4,
                                              void *user_data,
                                              on_mk_media_source_find_cb cb);

API_EXPORT mk_media_source API_CALL mk_media_source_find2(const char *schema,
                                                          const char *vhost,
                                                          const char *app,
                                                          const char *stream,
                                                          int from_mp4);
//MediaSource::for_each_media()
API_EXPORT void API_CALL mk_media_source_for_each(void *user_data, on_mk_media_source_find_cb cb, const char *schema,
                                                  const char *vhost, const char *app, const char *stream);

///////////////////////////////////////////HttpBody/////////////////////////////////////////////
// HttpBody对象的C映射  [AUTO-TRANSLATED:d8025ad9]
// cpp
//HttpBody对象的C映射
typedef struct mk_http_body_t *mk_http_body;
/**
 * 生成HttpStringBody
 * @param str 字符串指针
 * @param len 字符串长度，为0则用strlen获取
 * Generate HttpStringBody
 * @param str String pointer
 * @param len String length, if it is 0, use strlen to get it
 
 * [AUTO-TRANSLATED:7f828392]
 */
API_EXPORT mk_http_body API_CALL mk_http_body_from_string(const char *str,size_t len);

/**
 * 生成HttpBufferBody
 * @param buffer mk_buffer对象
 * Generate HttpBufferBody
 * @param buffer mk_buffer object
 
 * [AUTO-TRANSLATED:2d31a2e4]
 */
API_EXPORT mk_http_body API_CALL mk_http_body_from_buffer(mk_buffer buffer);


/**
 * 生成HttpFileBody
 * @param file_path 文件完整路径
 * Generate HttpFileBody
 * @param file_path File full path
 
 * [AUTO-TRANSLATED:4823ab7f]
 */
API_EXPORT mk_http_body API_CALL mk_http_body_from_file(const char *file_path);

/**
 * 生成HttpMultiFormBody
 * @param key_val 参数key-value
 * @param file_path 文件完整路径
 * Generate HttpMultiFormBody
 * @param key_val Parameter key-value
 * @param file_path File full path
 
 * [AUTO-TRANSLATED:17976911]
 */
API_EXPORT mk_http_body API_CALL mk_http_body_from_multi_form(const char *key_val[],const char *file_path);

/**
 * 销毁HttpBody
 * Destroy HttpBody
 
 * [AUTO-TRANSLATED:a1169b76]
 */
API_EXPORT void API_CALL mk_http_body_release(mk_http_body ctx);

///////////////////////////////////////////HttpResponseInvoker/////////////////////////////////////////////
// HttpSession::HttpResponseInvoker对象的C映射  [AUTO-TRANSLATED:89287e03]
// HttpSession::HttpResponseInvoker对象的C映射
typedef struct mk_http_response_invoker_t *mk_http_response_invoker;

/**
 * HttpSession::HttpResponseInvoker(const string &codeOut, const StrCaseMap &headerOut, const HttpBody::Ptr &body);
 * @param response_code 譬如200
 * @param response_header 返回的http头，譬如 {"Content-Type","text/html",NULL} 必须以NULL结尾
 * @param response_body body对象
 * HttpSession::HttpResponseInvoker(const string &codeOut, const StrCaseMap &headerOut, const HttpBody::Ptr &body);
 * @param response_code For example 200
 * @param response_header The returned http header, for example {"Content-Type","text/html",NULL} must end with NULL
 * @param response_body Body object
 
 * [AUTO-TRANSLATED:e006685a]
 */
API_EXPORT void API_CALL mk_http_response_invoker_do(const mk_http_response_invoker ctx,
                                                     int response_code,
                                                     const char **response_header,
                                                     const mk_http_body response_body);

/**
 * HttpSession::HttpResponseInvoker(const string &codeOut, const StrCaseMap &headerOut, const string &body);
 * @param response_code 譬如200
 * @param response_header 返回的http头，譬如 {"Content-Type","text/html",NULL} 必须以NULL结尾
 * @param response_content 返回的content部分，譬如一个网页内容
 * HttpSession::HttpResponseInvoker(const string &codeOut, const StrCaseMap &headerOut, const string &body);
 * @param response_code For example 200
 * @param response_header The returned http header, for example {"Content-Type","text/html",NULL} must end with NULL
 * @param response_content The returned content part, for example a web page content
 
 * [AUTO-TRANSLATED:0c3cf577]
 */
API_EXPORT void API_CALL mk_http_response_invoker_do_string(const mk_http_response_invoker ctx,
                                                            int response_code,
                                                            const char **response_header,
                                                            const char *response_content);
/**
 * HttpSession::HttpResponseInvoker(const StrCaseMap &requestHeader,const StrCaseMap &responseHeader,const string &filePath);
 * @param request_parser 请求事件中的mk_parser对象，用于提取其中http头中的Range字段，通过该字段先fseek然后再发送文件部分片段
 * @param response_header 返回的http头，譬如 {"Content-Type","text/html",NULL} 必须以NULL结尾
 * @param response_file_path 返回的content部分，譬如/path/to/html/file
 * HttpSession::HttpResponseInvoker(const StrCaseMap &requestHeader,const StrCaseMap &responseHeader,const string &filePath);
 * @param request_parser The mk_parser object in the request event, used to extract the Range field in the http header, use this field to fseek first and then send the file part fragment
 * @param response_header The returned http header, for example {"Content-Type","text/html",NULL} must end with NULL
 * @param response_file_path The returned content part, for example /path/to/html/file
 
 * [AUTO-TRANSLATED:8ed9ed9e]
 */
API_EXPORT void API_CALL mk_http_response_invoker_do_file(const mk_http_response_invoker ctx,
                                                          const mk_parser request_parser,
                                                          const char *response_header[],
                                                          const char *response_file_path);
/**
* 克隆mk_http_response_invoker对象，通过克隆对象为堆对象，可以实现跨线程异步执行mk_http_response_invoker_do
* 如果是同步执行mk_http_response_invoker_do，那么没必要克隆对象
 * Clone the mk_http_response_invoker object, by cloning the object to a heap object, you can achieve cross-thread asynchronous execution of mk_http_response_invoker_do
 * If you execute mk_http_response_invoker_do synchronously, then there is no need to clone the object
 
 * [AUTO-TRANSLATED:54c98395]
*/
API_EXPORT mk_http_response_invoker API_CALL mk_http_response_invoker_clone(const mk_http_response_invoker ctx);

/**
 * 销毁堆上的克隆对象
 * Destroy the cloned object on the heap
 
 * [AUTO-TRANSLATED:16c6a29b]
 */
API_EXPORT void API_CALL mk_http_response_invoker_clone_release(const mk_http_response_invoker ctx);

///////////////////////////////////////////HttpAccessPathInvoker/////////////////////////////////////////////
// HttpSession::HttpAccessPathInvoker对象的C映射  [AUTO-TRANSLATED:beb105f4]
// HttpSession::HttpAccessPathInvoker对象的C映射
typedef struct mk_http_access_path_invoker_t *mk_http_access_path_invoker;

/**
 * HttpSession::HttpAccessPathInvoker(const string &errMsg,const string &accessPath, int cookieLifeSecond);
 * @param err_msg 如果为空，则代表鉴权通过，否则为错误提示,可以为null
 * @param access_path 运行或禁止访问的根目录,可以为null
 * @param cookie_life_second 鉴权cookie有效期
 * HttpSession::HttpAccessPathInvoker(const string &errMsg,const string &accessPath, int cookieLifeSecond);
 * @param err_msg If it is empty, it means that the authentication is passed, otherwise it is an error prompt, it can be null
 * @param access_path The root directory to run or prohibit access, it can be null
 * @param cookie_life_second Authentication cookie validity period
 *
 * [AUTO-TRANSLATED:105c76c4]
 **/
API_EXPORT void API_CALL mk_http_access_path_invoker_do(const mk_http_access_path_invoker ctx,
                                                        const char *err_msg,
                                                        const char *access_path,
                                                        int cookie_life_second);

/**
* 克隆mk_http_access_path_invoker对象，通过克隆对象为堆对象，可以实现跨线程异步执行mk_http_access_path_invoker_do
* 如果是同步执行mk_http_access_path_invoker_do，那么没必要克隆对象
 * Clone the mk_http_access_path_invoker object, by cloning the object to a heap object, you can achieve cross-thread asynchronous execution of mk_http_access_path_invoker_do
 * If you execute mk_http_access_path_invoker_do synchronously, then there is no need to clone the object
 
 * [AUTO-TRANSLATED:ad2a71e4]
*/
API_EXPORT mk_http_access_path_invoker API_CALL mk_http_access_path_invoker_clone(const mk_http_access_path_invoker ctx);

/**
 * 销毁堆上的克隆对象
 * Destroy the cloned object on the heap
 
 * [AUTO-TRANSLATED:16c6a29b]
 */
API_EXPORT void API_CALL mk_http_access_path_invoker_clone_release(const mk_http_access_path_invoker ctx);

///////////////////////////////////////////RtspSession::onGetRealm/////////////////////////////////////////////
// RtspSession::onGetRealm对象的C映射  [AUTO-TRANSLATED:2355d645]
// RtspSession::onGetRealm对象的C映射
typedef struct mk_rtsp_get_realm_invoker_t *mk_rtsp_get_realm_invoker;
/**
 * 执行RtspSession::onGetRealm
 * @param realm 该rtsp流是否需要开启rtsp专属鉴权，至null或空字符串则不鉴权
 * Execute RtspSession::onGetRealm
 * @param realm Whether this rtsp stream needs to enable rtsp exclusive authentication, to null or empty string does not authenticate
 
 * [AUTO-TRANSLATED:ed88a88b]
 */
API_EXPORT void API_CALL mk_rtsp_get_realm_invoker_do(const mk_rtsp_get_realm_invoker ctx,
                                                      const char *realm);

/**
* 克隆mk_rtsp_get_realm_invoker对象，通过克隆对象为堆对象，可以实现跨线程异步执行mk_rtsp_get_realm_invoker_do
* 如果是同步执行mk_rtsp_get_realm_invoker_do，那么没必要克隆对象
 * Clone the mk_rtsp_get_realm_invoker object, by cloning the object to a heap object, you can achieve cross-thread asynchronous execution of mk_rtsp_get_realm_invoker_do
 * If you execute mk_rtsp_get_realm_invoker_do synchronously, then there is no need to clone the object
 
 * [AUTO-TRANSLATED:15fa6e77]
*/
API_EXPORT mk_rtsp_get_realm_invoker API_CALL mk_rtsp_get_realm_invoker_clone(const mk_rtsp_get_realm_invoker ctx);

/**
 * 销毁堆上的克隆对象
 * Destroy the cloned object on the heap
 
 * [AUTO-TRANSLATED:16c6a29b]
 */
API_EXPORT void API_CALL mk_rtsp_get_realm_invoker_clone_release(const mk_rtsp_get_realm_invoker ctx);

///////////////////////////////////////////RtspSession::onAuth/////////////////////////////////////////////
// RtspSession::onAuth对象的C映射  [AUTO-TRANSLATED:f3a1ebb7]
// RtspSession::onAuth对象的C映射
typedef struct mk_rtsp_auth_invoker_t *mk_rtsp_auth_invoker;

/**
 * 执行RtspSession::onAuth
 * @param encrypted 为true是则表明是md5加密的密码，否则是明文密码, 在请求明文密码时如果提供md5密码者则会导致认证失败
 * @param pwd_or_md5 明文密码或者md5加密的密码
 * Execute RtspSession::onAuth
 * @param encrypted If true, it means that the password is md5 encrypted, otherwise it is plain text password, if you provide md5 password when requesting plain text password, it will cause authentication failure
 * @param pwd_or_md5 Plain text password or md5 encrypted password
 
 * [AUTO-TRANSLATED:f7152252]
 */
API_EXPORT void API_CALL mk_rtsp_auth_invoker_do(const mk_rtsp_auth_invoker ctx,
                                                 int encrypted,
                                                 const char *pwd_or_md5);

/**
 * 克隆mk_rtsp_auth_invoker对象，通过克隆对象为堆对象，可以实现跨线程异步执行mk_rtsp_auth_invoker_do
 * 如果是同步执行mk_rtsp_auth_invoker_do，那么没必要克隆对象
 * Clone the mk_rtsp_auth_invoker object, by cloning the object to a heap object, you can achieve cross-thread asynchronous execution of mk_rtsp_auth_invoker_do
 * If you execute mk_rtsp_auth_invoker_do synchronously, then there is no need to clone the object
 
 * [AUTO-TRANSLATED:505859bd]
 */
API_EXPORT mk_rtsp_auth_invoker API_CALL mk_rtsp_auth_invoker_clone(const mk_rtsp_auth_invoker ctx);

/**
 * 销毁堆上的克隆对象
 * Destroy the cloned object on the heap
 
 * [AUTO-TRANSLATED:16c6a29b]
 */
API_EXPORT void API_CALL mk_rtsp_auth_invoker_clone_release(const mk_rtsp_auth_invoker ctx);

///////////////////////////////////////////Broadcast::PublishAuthInvoker/////////////////////////////////////////////
// Broadcast::PublishAuthInvoker对象的C映射  [AUTO-TRANSLATED:0eb37ee6]
// Broadcast::PublishAuthInvoker对象的C映射
typedef struct mk_publish_auth_invoker_t *mk_publish_auth_invoker;

/**
 * 执行Broadcast::PublishAuthInvoker
 * @param err_msg 为空或null则代表鉴权成功
 * @param enable_hls 是否允许转换hls
 * @param enable_mp4 是否运行MP4录制
 * Execute Broadcast::PublishAuthInvoker
 * @param err_msg Empty or null means authentication success
 * @param enable_hls Whether to allow hls conversion
 * @param enable_mp4 Whether to allow MP4 recording
 
 * [AUTO-TRANSLATED:ee8fb2b4]
 */
API_EXPORT void API_CALL mk_publish_auth_invoker_do(const mk_publish_auth_invoker ctx,
                                                    const char *err_msg,
                                                    int enable_hls,
                                                    int enable_mp4);

API_EXPORT void API_CALL mk_publish_auth_invoker_do2(const mk_publish_auth_invoker ctx, const char *err_msg, mk_ini option);

/**
 * 克隆mk_publish_auth_invoker对象，通过克隆对象为堆对象，可以实现跨线程异步执行mk_publish_auth_invoker_do
 * 如果是同步执行mk_publish_auth_invoker_do，那么没必要克隆对象
 * Clone the mk_publish_auth_invoker object, by cloning the object to a heap object, you can achieve cross-thread asynchronous execution of mk_publish_auth_invoker_do
 * If you execute mk_publish_auth_invoker_do synchronously, then there is no need to clone the object
 
 * [AUTO-TRANSLATED:03357111]
 */
API_EXPORT mk_publish_auth_invoker API_CALL mk_publish_auth_invoker_clone(const mk_publish_auth_invoker ctx);

/**
 * 销毁堆上的克隆对象
 * Destroy the cloned object on the heap
 
 * [AUTO-TRANSLATED:16c6a29b]
 */
API_EXPORT void API_CALL mk_publish_auth_invoker_clone_release(const mk_publish_auth_invoker ctx);

///////////////////////////////////////////Broadcast::AuthInvoker/////////////////////////////////////////////
// Broadcast::AuthInvoker对象的C映射  [AUTO-TRANSLATED:08f4186e]
// Broadcast::AuthInvoker对象的C映射
typedef struct mk_auth_invoker_t *mk_auth_invoker;

/**
 * 执行Broadcast::AuthInvoker
 * @param err_msg 为空或null则代表鉴权成功
 * Execute Broadcast::AuthInvoker
 * @param err_msg Empty or null means authentication success
 
 
 * [AUTO-TRANSLATED:7215fd9a]
 */
API_EXPORT void API_CALL mk_auth_invoker_do(const mk_auth_invoker ctx, const char *err_msg);

/**
 * 克隆mk_auth_invoker对象，通过克隆对象为堆对象，可以实现跨线程异步执行mk_auth_invoker_do
 * 如果是同步执行mk_auth_invoker_do，那么没必要克隆对象
 * Clone the mk_auth_invoker object. By cloning the object to a heap object, we can achieve asynchronous execution of mk_auth_invoker_do across threads.
 * If mk_auth_invoker_do is executed synchronously, there is no need to clone the object.
 
 * [AUTO-TRANSLATED:2430560d]
 */
API_EXPORT mk_auth_invoker API_CALL mk_auth_invoker_clone(const mk_auth_invoker ctx);

/**
 * 销毁堆上的克隆对象
 * Destroy the cloned object on the heap.
 
 * [AUTO-TRANSLATED:16c6a29b]
 */
API_EXPORT void API_CALL mk_auth_invoker_clone_release(const mk_auth_invoker ctx);

///////////////////////////////////////////WebRtcTransport/////////////////////////////////////////////
// WebRtcTransport对象的C映射  [AUTO-TRANSLATED:20b208cc]
// C mapping of the WebRtcTransport object
typedef struct mk_rtc_transport_t *mk_rtc_transport;

/**
 * 发送rtc数据通道
 * @param ctx 数据通道对象
 * @param streamId 流id
 * @param ppid 协议id
 * @param msg 数据
 * @param len 数据长度
 * Send rtc data channel
 * @param ctx Data channel object
 * @param streamId Stream id
 * @param ppid Protocol id
 * @param msg Data
 * @param len Data length
 
 
 * [AUTO-TRANSLATED:a0ce3c9e]
 */
API_EXPORT void API_CALL mk_rtc_send_datachannel(const mk_rtc_transport ctx, uint16_t streamId, uint32_t ppid, const char* msg, size_t len);

#ifdef __cplusplus
}
#endif
#endif //MK_EVENT_OBJECTS_H
