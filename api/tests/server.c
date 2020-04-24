/*
 * Copyright (c) 2016 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/xiongziliang/ZLMediaKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#include <signal.h>
#include <string.h>
#include "mk_mediakit.h"

#ifdef _WIN32
#include "windows.h"
#else

#include "unistd.h"

#endif

#define LOG_LEV 4

/**
 * 注册或反注册MediaSource事件广播
 * @param regist 注册为1，注销为0
 * @param sender 该MediaSource对象
 */
void API_CALL on_mk_media_changed(int regist,
                                  const mk_media_source sender) {
    log_printf(LOG_LEV,"%d %s/%s/%s/%s",(int)regist,
              mk_media_source_get_schema(sender),
              mk_media_source_get_vhost(sender),
              mk_media_source_get_app(sender),
              mk_media_source_get_stream(sender));

}

/**
 * 收到rtsp/rtmp推流事件广播，通过该事件控制推流鉴权
 * @see mk_publish_auth_invoker_do
 * @param url_info 推流url相关信息
 * @param invoker 执行invoker返回鉴权结果
 * @param sender 该tcp客户端相关信息
 */
void API_CALL on_mk_media_publish(const mk_media_info url_info,
                                  const mk_publish_auth_invoker invoker,
                                  const mk_sock_info sender) {
    char ip[64];
    log_printf(LOG_LEV,
               "client info, local: %s:%d, peer: %s:%d\n"
               "%s/%s/%s/%s, url params: %s",
               mk_sock_info_local_ip(sender,ip),
               mk_sock_info_local_port(sender),
               mk_sock_info_peer_ip(sender,ip + 32),
               mk_sock_info_peer_port(sender),
               mk_media_info_get_schema(url_info),
               mk_media_info_get_vhost(url_info),
               mk_media_info_get_app(url_info),
               mk_media_info_get_stream(url_info),
               mk_media_info_get_params(url_info));

    //允许推流，并且允许转rtxp/hls/mp4
    mk_publish_auth_invoker_do(invoker, NULL, 1, 1, 1);
}

/**
 * 播放rtsp/rtmp/http-flv/hls事件广播，通过该事件控制播放鉴权
 * @see mk_auth_invoker_do
 * @param url_info 播放url相关信息
 * @param invoker 执行invoker返回鉴权结果
 * @param sender 播放客户端相关信息
 */
void API_CALL on_mk_media_play(const mk_media_info url_info,
                               const mk_auth_invoker invoker,
                               const mk_sock_info sender) {

    char ip[64];
    log_printf(LOG_LEV,
               "client info, local: %s:%d, peer: %s:%d\n"
               "%s/%s/%s/%s, url params: %s",
               mk_sock_info_local_ip(sender,ip),
               mk_sock_info_local_port(sender),
               mk_sock_info_peer_ip(sender,ip + 32),
               mk_sock_info_peer_port(sender),
               mk_media_info_get_schema(url_info),
               mk_media_info_get_vhost(url_info),
               mk_media_info_get_app(url_info),
               mk_media_info_get_stream(url_info),
               mk_media_info_get_params(url_info));

    //允许播放
    mk_auth_invoker_do(invoker, NULL);
}

/**
 * 未找到流后会广播该事件，请在监听该事件后去拉流或其他方式产生流，这样就能按需拉流了
 * @param url_info 播放url相关信息
 * @param sender 播放客户端相关信息
 */
void API_CALL on_mk_media_not_found(const mk_media_info url_info,
                                    const mk_sock_info sender) {
    char ip[64];
    log_printf(LOG_LEV,
               "client info, local: %s:%d, peer: %s:%d\n"
               "%s/%s/%s/%s, url params: %s",
               mk_sock_info_local_ip(sender,ip),
               mk_sock_info_local_port(sender),
               mk_sock_info_peer_ip(sender,ip + 32),
               mk_sock_info_peer_port(sender),
               mk_media_info_get_schema(url_info),
               mk_media_info_get_vhost(url_info),
               mk_media_info_get_app(url_info),
               mk_media_info_get_stream(url_info),
               mk_media_info_get_params(url_info));
}

/**
 * 某个流无人消费时触发，目的为了实现无人观看时主动断开拉流等业务逻辑
 * @param sender 该MediaSource对象
 */
void API_CALL on_mk_media_no_reader(const mk_media_source sender) {
    log_printf(LOG_LEV,
               "%s/%s/%s/%s",
               mk_media_source_get_schema(sender),
               mk_media_source_get_vhost(sender),
               mk_media_source_get_app(sender),
               mk_media_source_get_stream(sender));
}

/**
 * 收到http api请求广播(包括GET/POST)
 * @param parser http请求内容对象
 * @param invoker 执行该invoker返回http回复
 * @param consumed 置1则说明我们要处理该事件
 * @param sender http客户端相关信息
 */
//测试url : http://127.0.0.1/api/test
void API_CALL on_mk_http_request(const mk_parser parser,
                                 const mk_http_response_invoker invoker,
                                 int *consumed,
                                 const mk_sock_info sender) {

    char ip[64];
    log_printf(LOG_LEV,
               "client info, local: %s:%d, peer: %s:%d\n"
               "%s %s?%s %s\n"
               "User-Agent: %s\n"
               "%s",
               mk_sock_info_local_ip(sender,ip),
               mk_sock_info_local_port(sender),
               mk_sock_info_peer_ip(sender,ip + 32),
               mk_sock_info_peer_port(sender),
               mk_parser_get_method(parser),
               mk_parser_get_url(parser),
               mk_parser_get_url_params(parser),
               mk_parser_get_tail(parser),
               mk_parser_get_header(parser, "User-Agent"),
               mk_parser_get_content(parser,NULL));

    const char *url = mk_parser_get_url(parser);
    if(strcmp(url,"/api/test") != 0){
        *consumed = 0;
        return;
    }

    //只拦截api: /api/test
    *consumed = 1;
    const char *response_header[] = {"Content-Type","text/html",NULL};
    const char *content =
                    "<html>"
                    "<head>"
                    "<title>hello world</title>"
                    "</head>"
                    "<body bgcolor=\"white\">"
                    "<center><h1>hello world</h1></center><hr>"
                    "<center>""ZLMediaKit-4.0</center>"
                    "</body>"
                    "</html>";
    mk_http_body body = mk_http_body_from_string(content,0);
    mk_http_response_invoker_do(invoker, "200 OK", response_header, body);
    mk_http_body_release(body);
}

/**
 * 在http文件服务器中,收到http访问文件或目录的广播,通过该事件控制访问http目录的权限
 * @param parser http请求内容对象
 * @param path 文件绝对路径
 * @param is_dir path是否为文件夹
 * @param invoker 执行invoker返回本次访问文件的结果
 * @param sender http客户端相关信息
 */
void API_CALL on_mk_http_access(const mk_parser parser,
                                const char *path,
                                int is_dir,
                                const mk_http_access_path_invoker invoker,
                                const mk_sock_info sender) {

    char ip[64];
    log_printf(LOG_LEV,
               "client info, local: %s:%d, peer: %s:%d, path: %s ,is_dir: %d\n"
               "%s %s?%s %s\n"
               "User-Agent: %s\n"
               "%s",
               mk_sock_info_local_ip(sender, ip),
               mk_sock_info_local_port(sender),
               mk_sock_info_peer_ip(sender, ip + 32),
               mk_sock_info_peer_port(sender),
               path,(int)is_dir,
               mk_parser_get_method(parser),
               mk_parser_get_url(parser),
               mk_parser_get_url_params(parser),
               mk_parser_get_tail(parser),
               mk_parser_get_header(parser,"User-Agent"),
               mk_parser_get_content(parser,NULL));

    //有访问权限,每次访问文件都需要鉴权
    mk_http_access_path_invoker_do(invoker, NULL, NULL, 0);
}

/**
 * 在http文件服务器中,收到http访问文件或目录前的广播,通过该事件可以控制http url到文件路径的映射
 * 在该事件中通过自行覆盖path参数，可以做到譬如根据虚拟主机或者app选择不同http根目录的目的
 * @param parser http请求内容对象
 * @param path 文件绝对路径,覆盖之可以重定向到其他文件
 * @param sender http客户端相关信息
 */
void API_CALL on_mk_http_before_access(const mk_parser parser,
                                       char *path,
                                       const mk_sock_info sender) {

    char ip[64];
    log_printf(LOG_LEV,
               "client info, local: %s:%d, peer: %s:%d, path: %s\n"
               "%s %s?%s %s\n"
               "User-Agent: %s\n"
               "%s",
               mk_sock_info_local_ip(sender,ip),
               mk_sock_info_local_port(sender),
               mk_sock_info_peer_ip(sender,ip + 32),
               mk_sock_info_peer_port(sender),
               path,
               mk_parser_get_method(parser),
               mk_parser_get_url(parser),
               mk_parser_get_url_params(parser),
               mk_parser_get_tail(parser),
               mk_parser_get_header(parser, "User-Agent"),
               mk_parser_get_content(parser,NULL));
    //覆盖path的值可以重定向文件
}

/**
 * 该rtsp流是否需要认证？是的话调用invoker并传入realm,否则传入空的realm
 * @param url_info 请求rtsp url相关信息
 * @param invoker 执行invoker返回是否需要rtsp专属认证
 * @param sender rtsp客户端相关信息
 */
void API_CALL on_mk_rtsp_get_realm(const mk_media_info url_info,
                                   const mk_rtsp_get_realm_invoker invoker,
                                   const mk_sock_info sender) {
    char ip[64];
    log_printf(LOG_LEV,
               "client info, local: %s:%d, peer: %s:%d\n"
               "%s/%s/%s/%s, url params: %s",
               mk_sock_info_local_ip(sender,ip),
               mk_sock_info_local_port(sender),
               mk_sock_info_peer_ip(sender,ip + 32),
               mk_sock_info_peer_port(sender),
               mk_media_info_get_schema(url_info),
               mk_media_info_get_vhost(url_info),
               mk_media_info_get_app(url_info),
               mk_media_info_get_stream(url_info),
               mk_media_info_get_params(url_info));

    //rtsp播放默认鉴权
    mk_rtsp_get_realm_invoker_do(invoker, "zlmediakit");
}

/**
 * 请求认证用户密码事件，user_name为用户名，must_no_encrypt如果为1，则必须提供明文密码(因为此时是base64认证方式),否则会导致认证失败
 * 获取到密码后请调用invoker并输入对应类型的密码和密码类型，invoker执行时会匹配密码
 * @param url_info 请求rtsp url相关信息
 * @param realm rtsp认证realm
 * @param user_name rtsp认证用户名
 * @param must_no_encrypt 如果为1，则必须提供明文密码(因为此时是base64认证方式),否则会导致认证失败
 * @param invoker  执行invoker返回rtsp专属认证的密码
 * @param sender rtsp客户端信息
 */
void API_CALL on_mk_rtsp_auth(const mk_media_info url_info,
                              const char *realm,
                              const char *user_name,
                              int must_no_encrypt,
                              const mk_rtsp_auth_invoker invoker,
                              const mk_sock_info sender) {

    char ip[64];
    log_printf(LOG_LEV,
               "client info, local: %s:%d, peer: %s:%d\n"
               "%s/%s/%s/%s, url params: %s\n"
               "realm: %s, user_name: %s, must_no_encrypt: %d",
               mk_sock_info_local_ip(sender,ip),
               mk_sock_info_local_port(sender),
               mk_sock_info_peer_ip(sender,ip + 32),
               mk_sock_info_peer_port(sender),
               mk_media_info_get_schema(url_info),
               mk_media_info_get_vhost(url_info),
               mk_media_info_get_app(url_info),
               mk_media_info_get_stream(url_info),
               mk_media_info_get_params(url_info),
               realm,user_name,(int)must_no_encrypt);

    //rtsp播放用户名跟密码一致
    mk_rtsp_auth_invoker_do(invoker,0,user_name);
}

/**
 * 录制mp4分片文件成功后广播
 */
void API_CALL on_mk_record_mp4(const mk_mp4_info mp4) {
    log_printf(LOG_LEV,
               "\nstart_time: %d\n"
               "time_len: %d\n"
               "file_size: %d\n"
               "file_path: %s\n"
               "file_name: %s\n"
               "folder: %s\n"
               "url: %s\n"
               "vhost: %s\n"
               "app: %s\n"
               "stream: %s\n",
               mk_mp4_info_get_start_time(mp4),
               mk_mp4_info_get_time_len(mp4),
               mk_mp4_info_get_file_size(mp4),
               mk_mp4_info_get_file_path(mp4),
               mk_mp4_info_get_file_name(mp4),
               mk_mp4_info_get_folder(mp4),
               mk_mp4_info_get_url(mp4),
               mk_mp4_info_get_vhost(mp4),
               mk_mp4_info_get_app(mp4),
               mk_mp4_info_get_stream(mp4));
}

/**
 * shell登录鉴权
 */
void API_CALL on_mk_shell_login(const char *user_name,
                                const char *passwd,
                                const mk_auth_invoker invoker,
                                const mk_sock_info sender) {

    char ip[64];
    log_printf(LOG_LEV,"client info, local: %s:%d, peer: %s:%d\n"
              "user_name: %s, passwd: %s",
              mk_sock_info_local_ip(sender,ip),
              mk_sock_info_local_port(sender),
              mk_sock_info_peer_ip(sender,ip + 32),
              mk_sock_info_peer_port(sender),
              user_name, passwd);
    //允许登录shell
    mk_auth_invoker_do(invoker, NULL);
}

/**
 * 停止rtsp/rtmp/http-flv会话后流量汇报事件广播
 * @param url_info 播放url相关信息
 * @param total_bytes 耗费上下行总流量，单位字节数
 * @param total_seconds 本次tcp会话时长，单位秒
 * @param is_player 客户端是否为播放器
 * @param peer_ip 客户端ip
 * @param peer_port 客户端端口号
 */
void API_CALL on_mk_flow_report(const mk_media_info url_info,
                                uint64_t total_bytes,
                                uint64_t total_seconds,
                                int is_player,
                                const mk_sock_info sender) {
    char ip[64];
    log_printf(LOG_LEV,"%s/%s/%s/%s, url params: %s,"
              "total_bytes: %d, total_seconds: %d, is_player: %d, peer_ip:%s, peer_port:%d",
               mk_media_info_get_schema(url_info),
               mk_media_info_get_vhost(url_info),
               mk_media_info_get_app(url_info),
               mk_media_info_get_stream(url_info),
               mk_media_info_get_params(url_info),
              (int)total_bytes,
              (int)total_seconds,
              (int)is_player,
              mk_sock_info_peer_ip(sender,ip),
              (int)mk_sock_info_peer_port(sender));
}

static int flag = 1;
static void s_on_exit(int sig){
    flag = 0;
}
int main(int argc, char *argv[]) {
    char *ini_path = mk_util_get_exe_dir("c_api.ini");
    char *ssl_path = mk_util_get_exe_dir("ssl.p12");

    mk_config config = {
            .ini = ini_path,
            .ini_is_path = 1,
            .log_level = 0,
            .log_file_path = NULL,
            .log_file_days = 0,
            .ssl = ssl_path,
            .ssl_is_path = 1,
            .ssl_pwd = NULL,
            .thread_num = 0
    };
    mk_env_init(&config);
    free(ini_path);
    free(ssl_path);

    mk_http_server_start(80, 0);
    mk_http_server_start(443, 1);
    mk_rtsp_server_start(554, 0);
    mk_rtmp_server_start(1935, 0);
    mk_shell_server_start(9000);
    mk_rtp_server_start(10000);

    mk_events events = {
            .on_mk_media_changed = on_mk_media_changed,
            .on_mk_media_publish = on_mk_media_publish,
            .on_mk_media_play = on_mk_media_play,
            .on_mk_media_not_found = on_mk_media_not_found,
            .on_mk_media_no_reader = on_mk_media_no_reader,
            .on_mk_http_request = on_mk_http_request,
            .on_mk_http_access = on_mk_http_access,
            .on_mk_http_before_access = on_mk_http_before_access,
            .on_mk_rtsp_get_realm = on_mk_rtsp_get_realm,
            .on_mk_rtsp_auth = on_mk_rtsp_auth,
            .on_mk_record_mp4 = on_mk_record_mp4,
            .on_mk_shell_login = on_mk_shell_login,
            .on_mk_flow_report = on_mk_flow_report
    };
    mk_events_listen(&events);

    signal(SIGINT, s_on_exit );// 设置退出信号
    while (flag) {
#ifdef _WIN32
        Sleep(1000);
#else
        sleep(1);
#endif
    }
    mk_stop_all_server();
    return 0;
}