/*
 * Copyright (c) 2016 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/xiongziliang/ZLMediaKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#ifndef ZLMEDIAKIT_HTTPFILEMANAGER_H
#define ZLMEDIAKIT_HTTPFILEMANAGER_H

#include "HttpBody.h"
#include "HttpCookie.h"
#include "Common/Parser.h"
#include "Network/TcpSession.h"
#include "Util/function_traits.h"

namespace mediakit {

class HttpResponseInvokerImp{
public:
    typedef std::function<void(const string &codeOut, const StrCaseMap &headerOut, const HttpBody::Ptr &body)> HttpResponseInvokerLambda0;
    typedef std::function<void(const string &codeOut, const StrCaseMap &headerOut, const string &body)> HttpResponseInvokerLambda1;

    HttpResponseInvokerImp(){}
    ~HttpResponseInvokerImp(){}
    template<typename C>
    HttpResponseInvokerImp(const C &c):HttpResponseInvokerImp(typename function_traits<C>::stl_function_type(c)) {}
    HttpResponseInvokerImp(const HttpResponseInvokerLambda0 &lambda);
    HttpResponseInvokerImp(const HttpResponseInvokerLambda1 &lambda);

    void operator()(const string &codeOut, const StrCaseMap &headerOut, const HttpBody::Ptr &body) const;
    void operator()(const string &codeOut, const StrCaseMap &headerOut, const string &body) const;
    void responseFile(const StrCaseMap &requestHeader,const StrCaseMap &responseHeader,const string &filePath) const;
    operator bool();
private:
    HttpResponseInvokerLambda0 _lambad;
};

/**
 * 该对象用于控制http静态文件夹服务器的访问权限
 */
class HttpFileManager  {
public:
    typedef function<void(const string &status_code, const string &content_type, const StrCaseMap &responseHeader, const HttpBody::Ptr &body)> invoker;

    /**
     * 访问文件或文件夹
     * @param sender 事件触发者
     * @param parser http请求
     * @param cb 回调对象
    */
    static void onAccessPath(TcpSession &sender, Parser &parser, const invoker &cb);

    /**
     * 获取mime值
     * @param name 文件后缀
     * @return mime值
     */
    static const string &getContentType(const char *name);
private:
    HttpFileManager() = delete;
    ~HttpFileManager() = delete;
};

}


#endif //ZLMEDIAKIT_HTTPFILEMANAGER_H
