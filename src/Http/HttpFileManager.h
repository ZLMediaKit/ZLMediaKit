/*
 * MIT License
 *
 * Copyright (c) 2016-2019 xiongziliang <771730766@qq.com>
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
