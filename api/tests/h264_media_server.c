/*
 * Copyright (c) 2016-present The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/ZLMediaKit/ZLMediaKit).
 *
 * Use of this source code is governed by MIT-like license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#include <signal.h>
#include <string.h>
#include <stdio.h>
#ifdef _WIN32
#include "windows.h"
#else
#include "unistd.h"
#endif
#include "mk_mediakit.h"

static int exit_flag = 0;
static void s_on_exit(int sig) {
    exit_flag = 1;
}

static void on_h264_frame(void *user_data, mk_h264_splitter splitter, const char *data, int size) {
#ifdef _WIN32
    Sleep(40);
#else
    usleep(40 * 1000);
#endif
    static int dts = 0;
    mk_frame frame = mk_frame_create(MKCodecH264, dts, dts, data, size, NULL, NULL);
    dts += 40;
    mk_media_input_frame((mk_media) user_data, frame);
    mk_frame_unref(frame);
}


//按照json转义规则转义webrtc answer sdp
static char *escape_string(const char *ptr){
    char *escaped = malloc(2 * strlen(ptr));
    char *ptr_escaped = escaped;
    while (1) {
        switch (*ptr) {
            case '\r': {
                *(ptr_escaped++) = '\\';
                *(ptr_escaped++) = 'r';
                break;
            }
            case '\n': {
                *(ptr_escaped++) = '\\';
                *(ptr_escaped++) = 'n';
                break;
            }
            case '\t': {
                *(ptr_escaped++) = '\\';
                *(ptr_escaped++) = 't';
                break;
            }

            default: {
                *(ptr_escaped++) = *ptr;
                if (!*ptr) {
                    return escaped;
                }
                break;
            }
        }
        ++ptr;
    }
}

static void on_mk_webrtc_get_answer_sdp_func(void *user_data, const char *answer, const char *err) {
    const char *response_header[] = { "Content-Type", "application/json", "Access-Control-Allow-Origin", "*" , NULL};
    if (answer) {
        answer = escape_string(answer);
    }
    size_t len = answer ? 2 * strlen(answer) : 1024;
    char *response_content = (char *)malloc(len);

    if (answer) {
        snprintf(response_content, len,
                 "{"
                 "\"sdp\":\"%s\","
                 "\"type\":\"answer\","
                 "\"code\":0"
                 "}",
                 answer);
    } else {
        snprintf(response_content, len,
                 "{"
                 "\"msg\":\"%s\","
                 "\"code\":-1"
                 "}",
                 err);
    }

    mk_http_response_invoker_do_string(user_data, 200, response_header, response_content);
    mk_http_response_invoker_clone_release(user_data);
    free(response_content);
    if (answer) {
        free((void*)answer);
    }
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

    const char *url = mk_parser_get_url(parser);
    *consumed = 1;

    if (strcmp(url, "/index/api/webrtc") == 0) {
        //拦截api: /index/api/webrtc
        char rtc_url[1024];
        snprintf(rtc_url, sizeof(rtc_url), "rtc://%s/%s/%s?%s", mk_parser_get_header(parser, "Host"),
                 mk_parser_get_url_param(parser, "app"), mk_parser_get_url_param(parser, "stream"),
                 mk_parser_get_url_params(parser));

        mk_webrtc_get_answer_sdp(mk_http_response_invoker_clone(invoker), on_mk_webrtc_get_answer_sdp_func,
                                 mk_parser_get_url_param(parser, "type"), mk_parser_get_content(parser, NULL), rtc_url);
    } else {
        *consumed = 0;
        return;
    }
}

int main(int argc, char *argv[]) {
    char *ini_path = mk_util_get_exe_dir("config.ini");
    mk_config config = {
            .ini = ini_path,
            .ini_is_path = 1,
            .log_level = 0,
            .log_mask = LOG_CONSOLE,
            .log_file_path = NULL,
            .log_file_days = 0,
            .ssl = NULL,
            .ssl_is_path = 1,
            .ssl_pwd = NULL,
            .thread_num = 0
    };
    mk_env_init(&config);
    mk_free(ini_path);

    mk_http_server_start(80, 0);
    mk_rtsp_server_start(554, 0);
    mk_rtmp_server_start(1935, 0);
    mk_rtc_server_start(atoi(mk_get_option("rtc.port")));

    mk_events events = {
            .on_mk_media_changed = NULL,
            .on_mk_media_publish = NULL,
            .on_mk_media_play = NULL,
            .on_mk_media_not_found = NULL,
            .on_mk_media_no_reader = NULL,
            .on_mk_http_request = on_mk_http_request,
            .on_mk_http_access = NULL,
            .on_mk_http_before_access = NULL,
            .on_mk_rtsp_get_realm = NULL,
            .on_mk_rtsp_auth = NULL,
            .on_mk_record_mp4 = NULL,
            .on_mk_shell_login = NULL,
            .on_mk_flow_report = NULL
    };
    mk_events_listen(&events);

    FILE *fp = fopen(argv[1], "rb");
    if (!fp) {
        log_error("打开文件失败!");
        return -1;
    }

    mk_media media = mk_media_create("__defaultVhost__", "live", "test", 0, 0, 0);
    //h264的codec
    //mk_media_init_video(media, 0, 0, 0, 0, 2 * 104 * 1024);
    codec_args v_args = {0};
    mk_track v_track = mk_track_create(MKCodecH264, &v_args);
    mk_media_init_track(media, v_track);
    mk_media_init_complete(media);
    mk_track_unref(v_track);

    //创建h264分帧器
    mk_h264_splitter splitter = mk_h264_splitter_create(on_h264_frame, media, 0);
    signal(SIGINT, s_on_exit);// 设置退出信号

    char buf[1024];
    while (!exit_flag) {
        int size = fread(buf, 1, sizeof(buf) - 1, fp);
        if (size > 0) {
            mk_h264_splitter_input_data(splitter, buf, size);
        } else {
            //文件读完了，重新开始
            fseek(fp, 0, SEEK_SET);
        }
    }

    log_info("文件读取完毕");
    mk_h264_splitter_release(splitter);
    mk_media_release(media);
    fclose(fp);
    mk_stop_all_server();
    return 0;
}