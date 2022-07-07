/*
 * Copyright (c) 2016 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/xia-chu/ZLMediaKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
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

int main(int argc, char *argv[]) {
    mk_config config = {
            .ini = NULL,
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
    mk_http_server_start(80, 0);
    mk_rtsp_server_start(554, 0);
    mk_rtmp_server_start(1935, 0);

    signal(SIGINT, s_on_exit);// 设置退出信号

    FILE *fp = fopen(argv[1], "rb");
    if (!fp) {
        log_error("打开文件失败!");
        return -1;
    }

    mk_media media = mk_media_create("__defaultVhost__", "live", "test", 0, 0, 0);
    //h264的codec
    //mk_media_init_video(media, 0, 0, 0, 0, 2 * 104 * 1024);
    codec_args v_args={0};
    mk_track v_track = mk_track_create(MKCodecH264,&v_args);
    mk_media_init_track(media,v_track);
    mk_media_init_complete(media);
    mk_track_unref(v_track);

    //创建h264分帧器
    mk_h264_splitter splitter = mk_h264_splitter_create(on_h264_frame, media);

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