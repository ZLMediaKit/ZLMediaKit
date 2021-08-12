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
#include "mk_mediakit.h"

#ifdef _WIN32
#include "windows.h"
#else
#include "unistd.h"
#endif

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

void API_CALL on_mk_play_event_func(void *user_data, int err_code, const char *err_msg) {
    Context *ctx = (Context *) user_data;
    release_media(&(ctx->media));
    release_pusher(&(ctx->pusher));
    if (err_code == 0) {
        //success
        log_debug("play success!");
        ctx->media = mk_media_create("__defaultVhost__", "live", "test", 0, 0, 0);

        int video_codec = mk_player_video_codec_id(ctx->player);
        int audio_codec = mk_player_audio_codec_id(ctx->player);
        if(video_codec != -1){
            mk_media_init_video(ctx->media, video_codec,
                                mk_player_video_width(ctx->player),
                                mk_player_video_height(ctx->player),
                                mk_player_video_fps(ctx->player));
        }

        if(audio_codec != -1){
            mk_media_init_audio(ctx->media,audio_codec,
                                mk_player_audio_samplerate(ctx->player),
                                mk_player_audio_channel(ctx->player),
                                mk_player_audio_bit(ctx->player));
        }
        mk_media_init_complete(ctx->media);
        mk_media_set_on_regist(ctx->media, on_mk_media_source_regist_func, ctx);

    } else {
        log_warn("play failed: %d %s", err_code, err_msg);
    }
}

void API_CALL on_mk_play_data_func(void *user_data,int track_type, int codec_id,void *data,size_t len, uint32_t dts,uint32_t pts){
    Context *ctx = (Context *) user_data;
    switch (codec_id) {
        case 0 : {
            //h264
            mk_media_input_h264(ctx->media, data, (int)len, dts, pts);
            break;
        }
        case 1 : {
            //h265
            mk_media_input_h265(ctx->media, data, (int)len, dts, pts);
            break;
        }
        case 2 : {
            //aac
            mk_media_input_aac(ctx->media, data, (int)len, dts, data);
            break;
        }
        case 3 : //g711a
        case 4 : //g711u
        case 5 : //opus
            mk_media_input_audio(ctx->media, data, (int) len, dts);
            break;

        default: {
            log_warn("unknown codec: %d", codec_id);
            break;
        }
    }
}

void context_start(Context *ctx, const char *url_pull, const char *url_push){
    release_player(&(ctx->player));
    ctx->player = mk_player_create();
    mk_player_set_on_result(ctx->player, on_mk_play_event_func, ctx);
    mk_player_set_on_shutdown(ctx->player, on_mk_play_event_func, ctx);
    mk_player_set_on_data(ctx->player, on_mk_play_data_func, ctx);
    mk_player_play(ctx->player, url_pull);
    strcpy(ctx->push_url, url_push);
}

//create_player("http://hls.weathertv.cn/tslslive/qCFIfHB/hls/live_sd.m3u8");

int main(int argc, char *argv[]){
    mk_config config = {
            .ini = NULL,
            .ini_is_path = 0,
            .log_level = 0,
            .ssl = NULL,
            .ssl_is_path = 1,
            .ssl_pwd = NULL,
            .thread_num = 0
    };
    mk_env_init(&config);

    //可以通过
    //rtmp://127.0.0.1/live/test
    //rtsp://127.0.0.1/live/test
    //播放mk_media的数据
    mk_rtsp_server_start(554, 0);
    mk_rtmp_server_start(1935, 0);

    Context *ctx = (Context *)malloc(sizeof(Context));
    memset(ctx, 0, sizeof(Context));

    //推流给自己测试，当然也可以推流给其他服务器测试
    context_start(ctx, "http://hls.weathertv.cn/tslslive/qCFIfHB/hls/live_sd.m3u8", "rtsp://127.0.0.1/live/rtsp_push");

    int i = 10 * 60;
    while(--i){
#ifdef _WIN32
        Sleep(1000);
#else
        sleep(1);
#endif
    }

    release_context(&ctx);
    return 0;
}