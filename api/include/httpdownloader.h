/*
 * MIT License
 *
 * Copyright (c) 2019 xiongziliang <771730766@qq.com>
 *
 * This file is part of ZLMediaKit(https://github.com/xiongziliang/ZLMediaKit).
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#ifndef MK_HTTP_DOWNLOADER_H_
#define MK_HTTP_DOWNLOADER_H_

#include "common.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef void *mk_http_downloader;

/**
 * @param user_data 用户数据指针
 * @param code 错误代码，0代表成功
 * @param err_msg 错误提示
 * @param file_path 文件保存路径
 */
typedef void(API_CALL *on_download_complete)(void *user_data, int code, const char *err_msg, const char *file_path);

/**
 * 创建http[s]下载器
 * @return 下载器指针
 */
API_EXPORT mk_http_downloader API_CALL mk_http_downloader_create();

/**
 * 销毁http[s]下载器
 * @param ctx 下载器指针
 */
API_EXPORT void API_CALL mk_http_downloader_release(mk_http_downloader ctx);

/**
 * 开始http[s]下载
 * @param ctx 下载器指针
 * @param url http[s]下载url
 * @param file 文件保存路径
 * @param cb 回调函数
 * @param user_data 用户数据指针
 */
API_EXPORT void API_CALL mk_http_downloader_start(mk_http_downloader ctx, const char *url, const char *file, on_download_complete cb, void *user_data);


#ifdef __cplusplus
}
#endif

#endif /* MK_HTTP_DOWNLOADER_H_ */
