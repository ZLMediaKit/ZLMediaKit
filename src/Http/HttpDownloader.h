/*
 * Copyright (c) 2016 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/xiongziliang/ZLMediaKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#ifndef SRC_HTTP_HTTPDOWNLOADER_H_
#define SRC_HTTP_HTTPDOWNLOADER_H_

#include "HttpClientImp.h"

namespace mediakit {

class HttpDownloader: public HttpClientImp {
public:
    typedef std::shared_ptr<HttpDownloader> Ptr;
    typedef std::function<void(ErrCode code,const string &errMsg,const string &filePath)> onDownloadResult;
    HttpDownloader();
    virtual ~HttpDownloader();
    //开始下载文件,默认断点续传方式下载
    void startDownload(const string &url,const string &filePath = "",bool bAppend = false, float timeOutSecond = 10 );
    void startDownload(const string &url,const onDownloadResult &cb,float timeOutSecond = 10){
        setOnResult(cb);
        startDownload(url,"",false,timeOutSecond);
    }
    void setOnResult(const onDownloadResult &cb){
        _onResult = cb;
    }
private:
    int64_t onResponseHeader(const string &status,const HttpHeader &headers) override;
    void onResponseBody(const char *buf,int64_t size,int64_t recvedSize,int64_t totalSize) override;
    void onResponseCompleted() override;
    void onDisconnect(const SockException &ex) override;
    void closeFile();
private:
    FILE *_saveFile = nullptr;
    string _filePath;
    onDownloadResult _onResult;
    bool _bDownloadSuccess = false;
};

} /* namespace mediakit */

#endif /* SRC_HTTP_HTTPDOWNLOADER_H_ */
