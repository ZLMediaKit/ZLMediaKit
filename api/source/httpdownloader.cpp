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
#include "httpdownloader.h"

#include "Util/logger.h"
#include "Http/HttpDownloader.h"
using namespace std;
using namespace toolkit;
using namespace mediakit;

API_EXPORT mk_http_downloader API_CALL mk_http_downloader_create() {
    HttpDownloader::Ptr *obj(new HttpDownloader::Ptr(new HttpDownloader()));
    return (mk_http_downloader) obj;
}

API_EXPORT void API_CALL mk_http_downloader_release(mk_http_downloader ctx) {
    assert(ctx);
    HttpDownloader::Ptr *obj = (HttpDownloader::Ptr *) ctx;
    delete obj;
}

API_EXPORT void API_CALL mk_http_downloader_start(mk_http_downloader ctx, const char *url, const char *file, on_mk_download_complete cb, void *user_data) {
    assert(ctx && url && file);
    HttpDownloader::Ptr *obj = (HttpDownloader::Ptr *) ctx;
    (*obj)->setOnResult([cb, user_data](ErrCode code, const string &errMsg, const string &filePath) {
        if (cb) {
            cb(user_data, code, errMsg.data(), filePath.data());
        }
    });
    (*obj)->startDownload(url, file, false);
}
