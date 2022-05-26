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
    mk_sem sem;
    mk_http_requester requester;
} Context;

void API_CALL on_requester_complete(void *user_data, int code, const char *err_msg){
    Context *ctx = (Context *)user_data;
    log_debug("code: %d %s", code, err_msg);
    size_t res_len = 0;
    log_debug("response: %s %s", mk_http_requester_get_response_status(ctx->requester),
              mk_http_requester_get_response_body(ctx->requester, &res_len));
    mk_sem_post(ctx->sem, 1);
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

    mk_http_requester requester = mk_http_requester_create();
    mk_http_requester_set_method(requester, "POST");

    mk_http_body body = mk_http_body_from_string("tn=monline_7_dg&ie=utf-8&wd=test", 0);
    mk_http_requester_set_body(requester, body);
    mk_http_body_release(body);

    mk_sem sem = mk_sem_create();

    Context ctx = {.requester = requester, .sem = sem};

    mk_http_requester_set_cb(requester, on_requester_complete, &ctx);
    mk_http_requester_start(requester, "http://www.baidu.com/baidu", 10);

    //等待http请求完毕
    mk_sem_wait(sem);

    mk_sem_release(sem);
    mk_http_requester_release(requester);
    return 0;
}