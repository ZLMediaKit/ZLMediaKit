/*
 * Copyright (c) 2021 The ZLToolKit project authors. All Rights Reserved.
 *
 * This file is part of ZLToolKit(https://github.com/ZLMediaKit/ZLToolKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#ifndef ZLTOOLKIT_SERVER_H
#define ZLTOOLKIT_SERVER_H

#include <unordered_map>
#include "Util/mini.h"
#include "Session.h"

namespace toolkit {

// 全局的 Session 记录对象, 方便后面管理
// 线程安全的
class SessionMap : public std::enable_shared_from_this<SessionMap> {
public:
    friend class SessionHelper;
    using Ptr = std::shared_ptr<SessionMap>;

    //单例
    static SessionMap &Instance();
    ~SessionMap() = default;

    //获取Session
    Session::Ptr get(const std::string &tag);
    void for_each_session(const std::function<void(const std::string &id, const Session::Ptr &session)> &cb);

private:
    SessionMap() = default;

    //移除Session
    bool del(const std::string &tag);
    //添加Session
    bool add(const std::string &tag, const Session::Ptr &session);

private:
    std::mutex _mtx_session;
    std::unordered_map<std::string, std::weak_ptr<Session> > _map_session;
};

class Server;

class SessionHelper {
public:
    bool enable = true;

    using Ptr = std::shared_ptr<SessionHelper>;

    SessionHelper(const std::weak_ptr<Server> &server, Session::Ptr session, std::string cls);
    ~SessionHelper();

    const Session::Ptr &session() const;
    const std::string &className() const;

private:
    std::string _cls;
    std::string _identifier;
    Session::Ptr _session;
    SessionMap::Ptr _session_map;
    std::weak_ptr<Server> _server;
};

// server 基类, 暂时仅用于剥离 SessionHelper 对 TcpServer 的依赖
// 后续将 TCP 与 UDP 服务通用部分加到这里.
class Server : public std::enable_shared_from_this<Server>, public mINI {
public:
    using Ptr = std::shared_ptr<Server>;

    explicit Server(EventPoller::Ptr poller = nullptr);
    virtual ~Server() = default;

protected:
    EventPoller::Ptr _poller;
};

} // namespace toolkit

#endif // ZLTOOLKIT_SERVER_H