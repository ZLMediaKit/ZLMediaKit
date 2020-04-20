/*
 * Copyright (c) 2016 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/xiongziliang/ZLMediaKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#include "Parser.h"

namespace mediakit{

string FindField(const char* buf, const char* start, const char *end ,int bufSize) {
    if(bufSize <=0 ){
        bufSize = strlen(buf);
    }
    const char *msg_start = buf, *msg_end = buf + bufSize;
    int len = 0;
    if (start != NULL) {
        len = strlen(start);
        msg_start = strstr(buf, start);
    }
    if (msg_start == NULL) {
        return "";
    }
    msg_start += len;
    if (end != NULL) {
        msg_end = strstr(msg_start, end);
        if (msg_end == NULL) {
            return "";
        }
    }
    return string(msg_start, msg_end);
}

Parser::Parser() {}
Parser::~Parser() {}

void Parser::Parse(const char *buf) {
    //解析
    const char *start = buf;
    Clear();
    while (true) {
        auto line = FindField(start, NULL, "\r\n");
        if (line.size() == 0) {
            break;
        }
        if (start == buf) {
            _strMethod = FindField(line.data(), NULL, " ");
            _strFullUrl = FindField(line.data(), " ", " ");
            auto args_pos = _strFullUrl.find('?');
            if (args_pos != string::npos) {
                _strUrl = _strFullUrl.substr(0, args_pos);
                _params = _strFullUrl.substr(args_pos + 1);
                _mapUrlArgs = parseArgs(_params);
            } else {
                _strUrl = _strFullUrl;
            }
            _strTail = FindField(line.data(), (_strFullUrl + " ").data(), NULL);
        } else {
            auto field = FindField(line.data(), NULL, ": ");
            auto value = FindField(line.data(), ": ", NULL);
            if (field.size() != 0) {
                _mapHeaders.emplace_force(field, value);
            }
        }
        start = start + line.size() + 2;
        if (strncmp(start, "\r\n", 2) == 0) { //协议解析完毕
            _strContent = FindField(start, "\r\n", NULL);
            break;
        }
    }
}

const string &Parser::Method() const {
    return _strMethod;
}

const string &Parser::Url() const {
    return _strUrl;
}

const string &Parser::FullUrl() const {
    return _strFullUrl;
}

const string &Parser::Tail() const {
    return _strTail;
}

const string &Parser::operator[](const char *name) const {
    auto it = _mapHeaders.find(name);
    if (it == _mapHeaders.end()) {
        return _strNull;
    }
    return it->second;
}

const string &Parser::Content() const {
    return _strContent;
}

void Parser::Clear() {
    _strMethod.clear();
    _strUrl.clear();
    _strFullUrl.clear();
    _params.clear();
    _strTail.clear();
    _strContent.clear();
    _mapHeaders.clear();
    _mapUrlArgs.clear();
}

const string &Parser::Params() const {
    return _params;
}

void Parser::setUrl(const string &url) {
    this->_strUrl = url;
}

void Parser::setContent(const string &content) {
    this->_strContent = content;
}

StrCaseMap &Parser::getHeader() const {
    return _mapHeaders;
}

StrCaseMap &Parser::getUrlArgs() const {
    return _mapUrlArgs;
}

StrCaseMap Parser::parseArgs(const string &str, const char *pair_delim, const char *key_delim) {
    StrCaseMap ret;
    auto arg_vec = split(str, pair_delim);
    for (string &key_val : arg_vec) {
        auto key = FindField(key_val.data(), NULL, key_delim);
        auto val = FindField(key_val.data(), key_delim, NULL);
        ret.emplace_force(trim(key), trim(val));
    }
    return ret;
}

}//namespace mediakit