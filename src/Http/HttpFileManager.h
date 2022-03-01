/*
 * Copyright (c) 2016 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/xia-chu/ZLMediaKit).
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
// 抽象异步http响应机制 @HttpSession
class HttpResponseInvokerImp{
public:
    typedef std::function<void(int code, const StrCaseMap &headerOut, const HttpBody::Ptr &body)> HttpResponseInvokerLambda0;
    typedef std::function<void(int code, const StrCaseMap &headerOut, const std::string &body)> HttpResponseInvokerLambda1;

    HttpResponseInvokerImp(){}
    ~HttpResponseInvokerImp(){}

    template<typename C>
    HttpResponseInvokerImp(const C &c):HttpResponseInvokerImp(typename toolkit::function_traits<C>::stl_function_type(c)) {}
    HttpResponseInvokerImp(const HttpResponseInvokerLambda0 &lambda);
    HttpResponseInvokerImp(const HttpResponseInvokerLambda1 &lambda);

    void operator()(int code, const StrCaseMap &headerOut, const toolkit::Buffer::Ptr &body) const;
    void operator()(int code, const StrCaseMap &headerOut, const HttpBody::Ptr &body) const;
    void operator()(int code, const StrCaseMap &headerOut, const std::string &body) const;
    void invoke(int code, const StrCaseMap &headerOut, const HttpBody::Ptr &body) const {
        if (_lambad) {
            _lambad(code, headerOut, body);
        }
    }
    void responseFile(const StrCaseMap &requestHeader, ///< 请求头部，用户读取Range信息
        const StrCaseMap &responseHeader, ///< 响应头部，会重新设置Content-Range
        const std::string &file, ///< 视is_path，可能是路径或文件内容
        bool use_mmap = true, ///< 是否使用内存映射文件
        bool is_path = true ///< file是否文件路径，否则是文件内容
    ) const;
    
    operator bool() {
        return _lambad.operator bool();
    }
private:

    HttpResponseInvokerLambda0 _lambad;
};

/**
 * 该对象用于控制http静态文件夹服务器的访问权限
 */
class HttpFileManager  {
public:
    typedef std::function<void(int code, const std::string &content_type, const StrCaseMap &responseHeader, const HttpBody::Ptr &body)> invoker;

    /**
     * 访问文件或文件夹
     * @param sender 事件触发者
     * @param parser http请求
     * @param cb 回调对象
    */
    static void onAccessPath(toolkit::TcpSession &sender, Parser &parser, const invoker &cb);

    /**
     * 获取mime值
     * @param name 文件后缀
     * @return mime值
     */
    static const std::string &getContentType(const char *name);
private:
    HttpFileManager() = delete;
    ~HttpFileManager() = delete;
};

}


#endif //ZLMEDIAKIT_HTTPFILEMANAGER_H
