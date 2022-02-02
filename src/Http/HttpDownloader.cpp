/*
 * Copyright (c) 2016 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/xia-chu/ZLMediaKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#include "HttpDownloader.h"
#include "Util/File.h"
#include "Util/MD5.h"
using namespace toolkit;
using namespace std;

namespace mediakit {

HttpDownloader::~HttpDownloader() {
    closeFile();
}

void HttpDownloader::startDownload(const string &url, const string &file_path, bool append) {
    _file_path = file_path;
    if (_file_path.empty()) {
        _file_path = exeDir() + "HttpDownloader/" + MD5(url).hexdigest();
    }
    _save_file = File::create_file(_file_path.data(), append ? "ab" : "wb");
    if (!_save_file) {
        auto strErr = StrPrinter << "打开文件失败:" << file_path << endl;
        throw std::runtime_error(strErr);
    }
    if (append) {
        auto currentLen = ftell(_save_file);
        if (currentLen) {
            //最少续传一个字节，怕遇到http 416的错误
            currentLen -= 1;
            fseek(_save_file, -1, SEEK_CUR);
        }
        addHeader("Range", StrPrinter << "bytes=" << currentLen << "-" << endl);
    }
    setMethod("GET");
    sendRequest(url);
}

void HttpDownloader::onResponseHeader(const string &status, const HttpHeader &headers) {
    if (status != "200" && status != "206") {
        //失败
        throw std::invalid_argument("bad http status: " + status);
    }
}

void HttpDownloader::onResponseBody(const char *buf, size_t size) {
    if (_save_file) {
        fwrite(buf, size, 1, _save_file);
    }
}

void HttpDownloader::onResponseCompleted(const SockException &ex) {
    closeFile();
    if (_on_result) {
        _on_result(ex, _file_path);
        _on_result = nullptr;
    }
}

void HttpDownloader::closeFile() {
    if (_save_file) {
        fflush(_save_file);
        fclose(_save_file);
        _save_file = nullptr;
    }
}

} /* namespace mediakit */
