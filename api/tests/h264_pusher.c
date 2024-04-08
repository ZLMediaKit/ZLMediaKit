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
#include <stdio.h>
#include <string.h>
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
    mk_media_input_frame((mk_media)user_data, frame);
    mk_frame_unref(frame);
}

typedef struct {
    mk_pusher pusher;
    char *url;
} Context;

void release_context(void *user_data) {
    Context *ptr = (Context *)user_data;
    if (ptr->pusher) {
        mk_pusher_release(ptr->pusher);
    }
    free(ptr->url);
    free(ptr);
    log_info("停止推流");
}

void on_push_result(void *user_data, int err_code, const char *err_msg) {
    Context *ptr = (Context *)user_data;
    if (err_code == 0) {
        log_info("推流成功: %s", ptr->url);
    } else {
        log_warn("推流%s失败: %d(%s)", ptr->url, err_code, err_msg);
    }
}

void on_push_shutdown(void *user_data, int err_code, const char *err_msg) {
    Context *ptr = (Context *)user_data;
    log_warn("推流%s中断: %d(%s)", ptr->url, err_code, err_msg);
}

void API_CALL on_regist(void *user_data, mk_media_source sender, int regist) {
    Context *ptr = (Context *)user_data;
    const char *schema = mk_media_source_get_schema(sender);
    if (strstr(ptr->url, schema) != ptr->url) {
        // 协议匹配失败
        return;
    }

    if (!regist) {
        // 注销
        if (ptr->pusher) {
            mk_pusher_release(ptr->pusher);
            ptr->pusher = NULL;
        }
    } else {
        // 注册
        if (!ptr->pusher) {
            ptr->pusher = mk_pusher_create_src(sender);
            mk_pusher_set_on_result2(ptr->pusher, on_push_result, ptr, NULL);
            mk_pusher_set_on_shutdown2(ptr->pusher, on_push_shutdown, ptr, NULL);
            // 开始推流
            mk_pusher_publish(ptr->pusher, ptr->url);
        }
    }
}

int main(int argc, char *argv[]) {
    if (argc < 3) {
        log_error("Usage: /path/to/h264/file rtsp_or_rtmp_url");
        return -1;
    }
    mk_config config = { .ini = NULL,
                         .ini_is_path = 1,
                         .log_level = 0,
                         .log_mask = LOG_CONSOLE,
                         .log_file_path = NULL,
                         .log_file_days = 0,
                         .ssl = NULL,
                         .ssl_is_path = 1,
                         .ssl_pwd = NULL,
                         .thread_num = 0 };
    mk_env_init(&config);

    FILE *fp = fopen(argv[1], "rb");
    if (!fp) {
        log_error("打开文件失败!");
        return -1;
    }

    mk_media media = mk_media_create("__defaultVhost__", "live", "test", 0, 0, 0);
    // h264的codec
    codec_args v_args = { 0 };
    mk_track v_track = mk_track_create(MKCodecH264, &v_args);
    mk_media_init_track(media, v_track);
    mk_media_init_complete(media);
    mk_track_unref(v_track);

    Context *ctx = (Context *)malloc(sizeof(Context));
    memset(ctx, 0, sizeof(Context));
    ctx->url = strdup(argv[2]);

    mk_media_set_on_regist2(media, on_regist, ctx, release_context);

    // 创建h264分帧器
    mk_h264_splitter splitter = mk_h264_splitter_create(on_h264_frame, media, 0);
    signal(SIGINT, s_on_exit); // 设置退出信号
    signal(SIGTERM, s_on_exit); // 设置退出信号

    char buf[1024];
    while (!exit_flag) {
        int size = fread(buf, 1, sizeof(buf) - 1, fp);
        if (size > 0) {
            mk_h264_splitter_input_data(splitter, buf, size);
        } else {
            // 文件读完了，重新开始
            fseek(fp, 0, SEEK_SET);
        }
    }

    log_info("文件读取完毕");
    mk_h264_splitter_release(splitter);
    mk_media_release(media);
    fclose(fp);
    return 0;
}
