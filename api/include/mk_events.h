﻿/*
 * Copyright (c) 2016-present The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/ZLMediaKit/ZLMediaKit).
 *
 * Use of this source code is governed by MIT-like license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MK_EVENTS_H
#define MK_EVENTS_H

#include "mk_common.h"
#include "mk_events_objects.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    /**
     * 注册或反注册MediaSource事件广播
     * @param regist 注册为1，注销为0
     * @param sender 该MediaSource对象
     */
    void (API_CALL *on_mk_media_changed)(int regist,
                                         const mk_media_source sender);

    /**
     * 收到rtsp/rtmp推流事件广播，通过该事件控制推流鉴权
     * @see mk_publish_auth_invoker_do
     * @param url_info 推流url相关信息
     * @param invoker 执行invoker返回鉴权结果
     * @param sender 该tcp客户端相关信息
     */
    void (API_CALL *on_mk_media_publish)(const mk_media_info url_info,
                                         const mk_publish_auth_invoker invoker,
                                         const mk_sock_info sender);

    /**
     * 播放rtsp/rtmp/http-flv/hls事件广播，通过该事件控制播放鉴权
     * @see mk_auth_invoker_do
     * @param url_info 播放url相关信息
     * @param invoker 执行invoker返回鉴权结果
     * @param sender 播放客户端相关信息
     */
    void (API_CALL *on_mk_media_play)(const mk_media_info url_info,
                                      const mk_auth_invoker invoker,
                                      const mk_sock_info sender);

    /**
     * 未找到流后会广播该事件，请在监听该事件后去拉流或其他方式产生流，这样就能按需拉流了
     * @param url_info 播放url相关信息
     * @param sender 播放客户端相关信息
     * @return 1 直接关闭
     *         0 等待流注册
     */
    int (API_CALL *on_mk_media_not_found)(const mk_media_info url_info,
                                           const mk_sock_info sender);

    /**
     * 某个流无人消费时触发，目的为了实现无人观看时主动断开拉流等业务逻辑
     * @param sender 该MediaSource对象
     */
    void (API_CALL *on_mk_media_no_reader)(const mk_media_source sender);

    /**
     * 收到http api请求广播(包括GET/POST)
     * @param parser http请求内容对象
     * @param invoker 执行该invoker返回http回复
     * @param consumed 置1则说明我们要处理该事件
     * @param sender http客户端相关信息
     */
    void (API_CALL *on_mk_http_request)(const mk_parser parser,
                                        const mk_http_response_invoker invoker,
                                        int *consumed,
                                        const mk_sock_info sender);

    /**
     * 在http文件服务器中,收到http访问文件或目录的广播,通过该事件控制访问http目录的权限
     * @param parser http请求内容对象
     * @param path 文件绝对路径
     * @param is_dir path是否为文件夹
     * @param invoker 执行invoker返回本次访问文件的结果
     * @param sender http客户端相关信息
     */
    void (API_CALL *on_mk_http_access)(const mk_parser parser,
                                       const char *path,
                                       int is_dir,
                                       const mk_http_access_path_invoker invoker,
                                       const mk_sock_info sender);

    /**
     * 在http文件服务器中,收到http访问文件或目录前的广播,通过该事件可以控制http url到文件路径的映射
     * 在该事件中通过自行覆盖path参数，可以做到譬如根据虚拟主机或者app选择不同http根目录的目的
     * @param parser http请求内容对象
     * @param path 文件绝对路径,覆盖之可以重定向到其他文件
     * @param sender http客户端相关信息
     */
    void (API_CALL *on_mk_http_before_access)(const mk_parser parser,
                                              char *path,
                                              const mk_sock_info sender);

    /**
     * 该rtsp流是否需要认证？是的话调用invoker并传入realm,否则传入空的realm
     * @param url_info 请求rtsp url相关信息
     * @param invoker 执行invoker返回是否需要rtsp专属认证
     * @param sender rtsp客户端相关信息
     */
    void (API_CALL *on_mk_rtsp_get_realm)(const mk_media_info url_info,
                                          const mk_rtsp_get_realm_invoker invoker,
                                          const mk_sock_info sender);

    /**
     * 请求认证用户密码事件，user_name为用户名，must_no_encrypt如果为true，则必须提供明文密码(因为此时是base64认证方式),否则会导致认证失败
     * 获取到密码后请调用invoker并输入对应类型的密码和密码类型，invoker执行时会匹配密码
     * @param url_info 请求rtsp url相关信息
     * @param realm rtsp认证realm
     * @param user_name rtsp认证用户名
     * @param must_no_encrypt 如果为true，则必须提供明文密码(因为此时是base64认证方式),否则会导致认证失败
     * @param invoker  执行invoker返回rtsp专属认证的密码
     * @param sender rtsp客户端信息
     */
    void (API_CALL *on_mk_rtsp_auth)(const mk_media_info url_info,
                                     const char *realm,
                                     const char *user_name,
                                     int must_no_encrypt,
                                     const mk_rtsp_auth_invoker invoker,
                                     const mk_sock_info sender);

    /**
     * 录制mp4分片文件成功后广播
     */
    void (API_CALL *on_mk_record_mp4)(const mk_record_info mp4);

     /**
     * 录制ts分片文件成功后广播
     */
    void (API_CALL *on_mk_record_ts)(const mk_record_info ts);

    /**
     * shell登录鉴权
     */
    void (API_CALL *on_mk_shell_login)(const char *user_name,
                                       const char *passwd,
                                       const mk_auth_invoker invoker,
                                       const mk_sock_info sender);

    /**
     * 停止rtsp/rtmp/http-flv会话后流量汇报事件广播
     * @param url_info 播放url相关信息
     * @param total_bytes 耗费上下行总流量，单位字节数
     * @param total_seconds 本次tcp会话时长，单位秒
     * @param is_player 客户端是否为播放器
     */
    void (API_CALL *on_mk_flow_report)(const mk_media_info url_info,
                                       size_t total_bytes,
                                       size_t total_seconds,
                                       int is_player,
                                       const mk_sock_info sender);


    /**
     * 日志输出广播
     * @param level 日志级别
     * @param file 源文件名
     * @param line 源文件行
     * @param function 源文件函数名
     * @param message 日志内容
     */
    void (API_CALL *on_mk_log)(int level, const char *file, int line, const char *function, const char *message);

    /**
     * 发送rtp流失败回调，适用于mk_media_source_start_send_rtp/mk_media_start_send_rtp接口触发的rtp发送
     * @param vhost 虚拟主机
     * @param app 应用名
     * @param stream 流id
     * @param ssrc ssrc的10进制打印，通过atoi转换为整型
     * @param err 错误代码
     * @param msg 错误提示
     */
    void (API_CALL *on_mk_media_send_rtp_stop)(const char *vhost, const char *app, const char *stream, const char *ssrc, int err, const char *msg);

    /**
     * rtc sctp连接中/完成/失败/关闭回调
     * @param rtc_transport 数据通道对象
     */
    void (API_CALL *on_mk_rtc_sctp_connecting)(mk_rtc_transport rtc_transport);
    void (API_CALL *on_mk_rtc_sctp_connected)(mk_rtc_transport rtc_transport);
    void (API_CALL *on_mk_rtc_sctp_failed)(mk_rtc_transport rtc_transport);
    void (API_CALL *on_mk_rtc_sctp_closed)(mk_rtc_transport rtc_transport);

    /**
     * rtc数据通道发送数据回调
     * @param rtc_transport 数据通道对象
     * @param msg 数据
     * @param len 数据长度
     */
    void (API_CALL *on_mk_rtc_sctp_send)(mk_rtc_transport rtc_transport, const uint8_t *msg, size_t len);

    /**
     * rtc数据通道接收数据回调
     * @param rtc_transport 数据通道对象
     * @param streamId 流id
     * @param ppid 协议id
     * @param msg 数据
     * @param len 数据长度
     */
    void (API_CALL *on_mk_rtc_sctp_received)(mk_rtc_transport rtc_transport, uint16_t streamId, uint32_t ppid, const uint8_t *msg, size_t len);

} mk_events;


/**
 * 监听ZLMediaKit里面的事件
 * @param events 各个事件的结构体,这个对象在内部会再拷贝一次，可以设置为null以便取消监听
 */
API_EXPORT void API_CALL mk_events_listen(const mk_events *events);


#ifdef __cplusplus
}
#endif
#endif //MK_EVENTS_H
