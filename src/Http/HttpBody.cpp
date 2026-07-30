/*
 * Copyright (c) 2016-present The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/ZLMediaKit/ZLMediaKit).
 *
 * Use of this source code is governed by MIT-like license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#include "HttpBody.h"
#include "HttpClient.h"
#include "Common/macros.h"
#include "Util/uv_errno.h"

using namespace std;
using namespace toolkit;

namespace mediakit {

HttpStringBody::HttpStringBody(string str) {
    _str = std::move(str);
}

int64_t HttpStringBody::remainSize() {
    return _str.size() - _offset;
}

Buffer::Ptr HttpStringBody::readData(size_t size) {
    size = MIN((size_t)remainSize(), size);
    if (!size) {
        // 没有剩余字节了  [AUTO-TRANSLATED:7bbaa343]
        // No remaining bytes
        return nullptr;
    }
    auto ret = std::make_shared<BufferString>(_str, _offset, size);
    _offset += size;
    return ret;
}

HttpMultiFormBody::HttpMultiFormBody(const HttpArgs &args, const string &filePath, const string &boundary) {
    _fileBody = std::make_shared<HttpFileBody>(filePath);
    if (_fileBody->remainSize() < 0) {
        throw std::invalid_argument(StrPrinter << "open file failed：" << filePath << " " << get_uv_errmsg());
    }

    auto fileName = filePath;
    auto pos = filePath.rfind('/');
    if (pos != string::npos) {
        fileName = filePath.substr(pos + 1);
    }
    _bodyPrefix = multiFormBodyPrefix(args, boundary, fileName);
    _bodySuffix = multiFormBodySuffix(boundary);
    _totalSize = _bodyPrefix.size() + _bodySuffix.size() + _fileBody->remainSize();
}

int64_t HttpMultiFormBody::remainSize() {
    return _totalSize - _offset;
}

Buffer::Ptr HttpMultiFormBody::readData(size_t size) {
    if (_bodyPrefix.size()) {
        auto ret = std::make_shared<BufferString>(_bodyPrefix);
        _offset += _bodyPrefix.size();
        _bodyPrefix.clear();
        return ret;
    }

    if (_fileBody->remainSize()) {
        auto ret = _fileBody->readData(size);
        if (!ret) {
            // 读取文件出现异常，提前中断  [AUTO-TRANSLATED:5b8052d9]
            // An exception occurred while reading the file, and the process was interrupted prematurely
            _offset = _totalSize;
        } else {
            _offset += ret->size();
        }
        return ret;
    }

    if (_bodySuffix.size()) {
        auto ret = std::make_shared<BufferString>(_bodySuffix);
        _offset = _totalSize;
        _bodySuffix.clear();
        return ret;
    }

    return nullptr;
}

string HttpMultiFormBody::multiFormBodySuffix(const string &boundary) {
    return "\r\n--" + boundary + "--";
}

string HttpMultiFormBody::multiFormContentType(const string &boundary) {
    return StrPrinter << "multipart/form-data; boundary=" << boundary;
}

string HttpMultiFormBody::multiFormBodyPrefix(const HttpArgs &args, const string &boundary, const string &fileName) {
    string MPboundary = string("--") + boundary;
    _StrPrinter body;
    for (auto &pr : args) {
        body << MPboundary << "\r\n";
        body << "Content-Disposition: form-data; name=\"" << pr.first << "\"\r\n\r\n";
        body << pr.second << "\r\n";
    }
    body << MPboundary << "\r\n";
    body << "Content-Disposition: form-data; name=\""
         << "file"
         << "\"; filename=\"" << fileName << "\"\r\n";
    body << "Content-Type: application/octet-stream\r\n\r\n";
    return body;
}

HttpBufferBody::HttpBufferBody(Buffer::Ptr buffer) {
    _buffer = std::move(buffer);
}

int64_t HttpBufferBody::remainSize() {
    return _buffer ? _buffer->size() : 0;
}

Buffer::Ptr HttpBufferBody::readData(size_t size) {
    return Buffer::Ptr(std::move(_buffer));
}

} // namespace mediakit
