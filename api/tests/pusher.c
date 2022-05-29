/*
 * Copyright (c) 2016 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/xia-chu/ZLMediaKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#include <string.h>
#include "mk_mediakit.h"

typedef struct {
    mk_player player;
    mk_media media;
    mk_pusher pusher;
    char push_url[1024];
} Context;

void release_media(mk_media *ptr) {
    if (ptr && *ptr) {
        mk_media_release(*ptr);
        *ptr = NULL;
    }
}

void release_player(mk_player *ptr) {
    if (ptr && *ptr) {
        mk_player_release(*ptr);
        *ptr = NULL;
    }
}

void release_pusher(mk_media *ptr) {
    if (ptr && *ptr) {
        mk_pusher_release(*ptr);
        *ptr = NULL;
    }
}

void release_context(Context **ptr){
    if (ptr && *ptr) {
        release_pusher(&(*ptr)->pusher);
        release_media(&(*ptr)->media);
        release_player(&(*ptr)->player);
        free(*ptr);
        *ptr = NULL;
    }
}

void API_CALL on_mk_push_event_func(void *user_data,int err_code,const char *err_msg){
    Context *ctx = (Context *) user_data;
    if (err_code == 0) {
        //push success
        log_info("push %s success!", ctx->push_url);
    } else {
        log_warn("push %s failed:%d %s", ctx->push_url, err_code, err_msg);
        release_pusher(&(ctx->pusher));
    }
}

void API_CALL on_mk_media_source_regist_func(void *user_data, mk_media_source sender, int regist){
    Context *ctx = (Context *) user_data;
    const char *schema = mk_media_source_get_schema(sender);
    if (strncmp(schema, ctx->push_url, strlen(schema)) == 0) {
        //判断是否为推流协议相关的流注册或注销事件
        release_pusher(&(ctx->pusher));
        if (regist) {
            ctx->pusher = mk_pusher_create_src(sender);
            mk_pusher_set_on_result(ctx->pusher, on_mk_push_event_func, ctx);
            mk_pusher_set_on_shutdown(ctx->pusher, on_mk_push_event_func, ctx);
            mk_pusher_publish(ctx->pusher, ctx->push_url);
        } else {
            log_info("push stoped!");
        }
    }
}

void API_CALL on_track_frame_out(void *user_data, mk_frame frame) {
    Context *ctx = (Context *) user_data;
    mk_media_input_frame(ctx->media, frame);
}

void API_CALL on_mk_play_event_func(void *user_data, int err_code, const char *err_msg, mk_track tracks[], int track_count) {
    Context *ctx = (Context *) user_data;
    release_media(&(ctx->media));
    release_pusher(&(ctx->pusher));
    if (err_code == 0) {
        //success
        log_debug("play success!");
        ctx->media = mk_media_create("__defaultVhost__", "live", "test", 0, 0, 0);
        int i;
        for (i = 0; i < track_count; ++i) {
            mk_media_init_track(ctx->media, tracks[i]);
            mk_track_add_delegate(tracks[i], on_track_frame_out, user_data);
        }
        mk_media_init_complete(ctx->media);
        mk_media_set_on_regist(ctx->media, on_mk_media_source_regist_func, ctx);

    } else {
        log_warn("play failed: %d %s", err_code, err_msg);
    }
}

void context_start(Context *ctx, const char *url_pull, const char *url_push){
    release_player(&(ctx->player));
    ctx->player = mk_player_create();
    mk_player_set_on_result(ctx->player, on_mk_play_event_func, ctx);
    mk_player_set_on_shutdown(ctx->player, on_mk_play_event_func, ctx);
    mk_player_play(ctx->player, url_pull);
    strcpy(ctx->push_url, url_push);
}

int main(int argc, char *argv[]){
    mk_config config = {
            .ini = NULL,
            .ini_is_path = 0,
            .log_level = 0,
            .log_mask = LOG_CONSOLE,
            .ssl = NULL,
            .ssl_is_path = 1,
            .ssl_pwd = NULL,
            .thread_num = 0
    };
    mk_env_init(&config);

    if (argc != 3) {
        printf("Usage: ./pusher.c pull_url push_url\n");
        return -1;
    }

    //可以通过
    //rtmp://127.0.0.1/live/test
    //rtsp://127.0.0.1/live/test
    //播放mk_media的数据
    mk_rtsp_server_start(554, 0);
    mk_rtmp_server_start(1935, 0);

    Context *ctx = (Context *) malloc(sizeof(Context));
    memset(ctx, 0, sizeof(Context));

    //推流给自己测试，当然也可以推流给其他服务器测试
    context_start(ctx, argv[1], argv[2]);

    log_info("enter any key to exit");
    getchar();

    release_context(&ctx);
    return 0;
}