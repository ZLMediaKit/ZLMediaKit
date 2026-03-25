/*
 * Copyright (c) 2016-present The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/ZLMediaKit/ZLMediaKit).
 *
 * Use of this source code is governed by MIT-like license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#ifndef ZLMEDIAKIT_HTTPFILEMANAGER_H
#define ZLMEDIAKIT_HTTPFILEMANAGER_H

#include "HttpCookie.h"
#include "HttpServerTypes.h"
#include "Network/Session.h"

namespace mediakit {

/**
 * 该对象用于控制http静态文件夹服务器的访问权限
 * This object is used to control access permissions for the http static folder server.
 */
class HttpFileManager {
public:
    typedef std::function<void(int code, const std::string &content_type, const StrCaseMap &responseHeader, const HttpBody::Ptr &body)> invoker;

    /**
     * 访问文件或文件夹
     * @param sender 事件触发者
     * @param parser http请求
     * @param cb 回调对象
     * Access files or folders
     * @param sender Event trigger
     * @param parser http request
     * @param cb Callback object
     */
    static void onAccessPath(toolkit::Session &sender, Parser &parser, const invoker &cb);
    static void onAccessPath(toolkit::SockInfo &sender, Parser &parser, const invoker &cb, toolkit::Session *session);

    /**
     * 获取mime值
     * @param name 文件后缀
     * @return mime值
     * Get mime value
     * @param name File suffix
     * @return mime value
     */
    static const std::string &getContentType(const char *name);

    /**
     * 该ip是否再白名单中
     * @param ip 支持ipv4和ipv6
     * Whether this ip is in the whitelist
     * @param ip Supports ipv4 and ipv6
     */
    static bool isIPAllowed(const std::string &ip);

private:
    HttpFileManager() = delete;
    ~HttpFileManager() = delete;
};

} // namespace mediakit

#endif // ZLMEDIAKIT_HTTPFILEMANAGER_H
