/*
 * Copyright (c) 2016 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/xiongziliang/ZLMediaKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#ifndef ZLMEDIAKIT_PARSER_H
#define ZLMEDIAKIT_PARSER_H

#include <map>
#include <string>
#include "Util/util.h"
using namespace std;
using namespace toolkit;

namespace mediakit{

string FindField(const char *buf, const char *start, const char *end, int bufSize = 0);

struct StrCaseCompare {
    bool operator()(const string &__x, const string &__y) const {
        return strcasecmp(__x.data(), __y.data()) < 0;
    }
};


class StrCaseMap : public multimap<string, string, StrCaseCompare>{
    public:
    typedef multimap<string, string, StrCaseCompare> Super ;
    StrCaseMap() = default;
    ~StrCaseMap() = default;

    string &operator[](const string &k){
        auto it = find(k);
        if(it == end()){
            it = Super::emplace(k,"");
        }
        return it->second;
    }

    template <typename V>
    void emplace(const string &k, V &&v) {
        auto it = find(k);
        if(it != end()){
            return;
        }
        Super::emplace(k,std::forward<V>(v));
    }

    template <typename V>
    void emplace_force(const string k , V &&v) {
        Super::emplace(k,std::forward<V>(v));
    }
};

//rtsp/http/sip解析类
class Parser {
public:
    Parser();
    ~Parser();
    //解析信令
    void Parse(const char *buf);
    //获取命令字
    const string &Method() const;
    //获取中间url，不包含?后面的参数
    const string &Url() const;
    //获取中间url，包含?后面的参数
    const string &FullUrl() const;
    //获取命令协议名
    const string &Tail() const;
    //根据header key名，获取请求header value值
    const string &operator[](const char *name) const;
    //获取http body或sdp
    const string &Content() const;
    //清空，为了重用
    void Clear();
    //获取?后面的参数
    const string &Params() const;
    //重新设置url
    void setUrl(const string &url);
    //重新设置content
    void setContent(const string &content);
    //获取header列表
    StrCaseMap &getHeader() const;
    //获取url参数列表
    StrCaseMap &getUrlArgs() const;
    //解析?后面的参数
    static StrCaseMap parseArgs(const string &str, const char *pair_delim = "&", const char *key_delim = "=");
private:
    string _strMethod;
    string _strUrl;
    string _strTail;
    string _strContent;
    string _strNull;
    string _strFullUrl;
    string _params;
    mutable StrCaseMap _mapHeaders;
    mutable StrCaseMap _mapUrlArgs;
};


}//namespace mediakit

#endif //ZLMEDIAKIT_PARSER_H
