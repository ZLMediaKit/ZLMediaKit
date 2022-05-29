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
#include <stdlib.h>
#include <stdio.h>
#include "mk_mediakit.h"

typedef struct {
    mk_player player;
    mk_decoder video_decoder;
    mk_swscale swscale;
} Context;

void API_CALL on_track_frame_out(void *user_data, mk_frame frame) {
    Context *ctx = (Context *) user_data;
    mk_decoder_decode(ctx->video_decoder, frame, 1, 1);
}

void API_CALL on_frame_decode(void *user_data, mk_frame_pix frame) {
    Context *ctx = (Context *) user_data;
    int w = mk_get_av_frame_width(mk_frame_pix_get_av_frame(frame));
    int h = mk_get_av_frame_height(mk_frame_pix_get_av_frame(frame));

#if 1
    uint8_t *brg24 = malloc(w * h * 3);
    mk_swscale_input_frame(ctx->swscale, frame, brg24);
    free(brg24);
#else
    //todo 此处转换为opencv对象
    cv::Mat *mat = new cv::Mat();
    mat->create(h, w, CV_8UC3);
    mk_swscale_input_frame(ctx->swscale, frame,  (uint8_t *) mat->data);
#endif
    log_trace("decode frame output, pts:%d, w:%d, h:%d", mk_get_av_frame_dts(mk_frame_pix_get_av_frame(frame)), w, h);
}

void API_CALL on_mk_play_event_func(void *user_data, int err_code, const char *err_msg, mk_track tracks[],
                                    int track_count) {
    Context *ctx = (Context *) user_data;
    if (err_code == 0) {
        //success
        log_debug("play success!");
        int i;
        for (i = 0; i < track_count; ++i) {
            if (mk_track_is_video(tracks[i])) {
                log_info("got video track: %s", mk_track_codec_name(tracks[i]));
                ctx->video_decoder = mk_decoder_create(tracks[i], 0);
                mk_decoder_set_cb(ctx->video_decoder, on_frame_decode, user_data);
                //监听track数据回调
                mk_track_add_delegate(tracks[i], on_track_frame_out, user_data);
            }
        }
    } else {
        log_warn("play failed: %d %s", err_code, err_msg);
    }
}

void API_CALL on_mk_shutdown_func(void *user_data, int err_code, const char *err_msg, mk_track tracks[], int track_count) {
    log_warn("play interrupted: %d %s", err_code, err_msg);
}

int main(int argc, char *argv[]) {
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
    if (argc != 2) {
        printf("Usage: ./player url\n");
        return -1;
    }

    Context ctx;
    memset(&ctx, 0, sizeof(Context));

    ctx.player = mk_player_create();
    ctx.swscale = mk_swscale_create(3, 0, 0);
    mk_player_set_on_result(ctx.player, on_mk_play_event_func, &ctx);
    mk_player_set_on_shutdown(ctx.player, on_mk_shutdown_func, &ctx);
    mk_player_play(ctx.player, argv[1]);

    log_info("enter any key to exit");
    getchar();

    if (ctx.player) {
        mk_player_release(ctx.player);
    }
    if (ctx.video_decoder) {
        mk_decoder_release(ctx.video_decoder, 1);
    }
    if (ctx.swscale) {
        mk_swscale_release(ctx.swscale);
    }
    return 0;
}