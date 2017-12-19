/*
 * MIT License
 *
 * Copyright (c) 2016 xiongziliang <771730766@qq.com>
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

#ifndef SRC_HTTPDOWNLOADER_H_
#define SRC_HTTPDOWNLOADER_H_

#include "common.h"

#ifdef __cplusplus
extern "C" {
#endif

////////////////////////////////////////Httpdownloader/////////////////////////////////////////////////
typedef void * HttpDownloaderContex;
typedef void(API_CALL *downloader_onResult)(void *userData,int code,const char *errMsg,const char *filePath);

API_EXPORT HttpDownloaderContex API_CALL createDownloader();
API_EXPORT void API_CALL downloader_startDownload(HttpDownloaderContex ctx,const char *url,downloader_onResult cb,void *userData);
API_EXPORT void API_CALL releaseDownloader(HttpDownloaderContex ctx);


#ifdef __cplusplus
}
#endif

#endif /* SRC_HTTPDOWNLOADER_H_ */
