/*
 * Copyright (c) 2016 The ZLToolKit project authors. All Rights Reserved.
 *
 * This file is part of ZLToolKit(https://github.com/ZLMediaKit/ZLToolKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#ifndef TCPSERVER_TCPSERVER_H
#define TCPSERVER_TCPSERVER_H

#include <memory>
#include <functional>
#include <unordered_map>
#include "Server.h"
#include "Session.h"
#include "Poller/Timer.h"
#include "Util/util.h"

namespace toolkit {

//TCP服务器，可配置的；配置通过Session::attachServer方法传递给会话对象
class TcpServer : public Server {
public:
    using Ptr = std::shared_ptr<TcpServer>;

    /**
     * 创建tcp服务器，listen fd的accept事件会加入到所有的poller线程中监听
     * 在调用TcpServer::start函数时，内部会创建多个子TcpServer对象，
     * 这些子TcpServer对象通过Socket对象克隆的方式在多个poller线程中监听同一个listen fd
     * 这样这个TCP服务器将会通过抢占式accept的方式把客户端均匀的分布到不同的poller线程
     * 通过该方式能实现客户端负载均衡以及提高连接接收速度
     */
    explicit TcpServer(const EventPoller::Ptr &poller = nullptr);
    ~TcpServer() override;

    /**
    * @brief 开始tcp server
    * @param port 本机端口，0则随机
    * @param host 监听网卡ip
    * @param backlog tcp listen backlog
    */
    template<typename SessionType>
    void start(uint16_t port, const std::string &host = "::", uint32_t backlog = 1024) {
        static std::string cls_name = toolkit::demangle(typeid(SessionType).name());
        //Session创建器，通过它创建不同类型的服务器
        _session_alloc = [](const TcpServer::Ptr &server, const Socket::Ptr &sock) {
            auto session = std::shared_ptr<SessionType>(new SessionType(sock), [](SessionType * ptr) {
                TraceP(static_cast<Session *>(ptr)) << "~" << cls_name;
                delete ptr;
            });
            TraceP(static_cast<Session *>(session.get())) << cls_name;
            session->setOnCreateSocket(server->_on_create_socket);
            return std::make_shared<SessionHelper>(server, std::move(session), cls_name);
        };
        start_l(port, host, backlog);
    }

    /**
     * @brief 获取服务器监听端口号, 服务器可以选择监听随机端口
     */
    uint16_t getPort();

    /**
     * @brief 自定义socket构建行为
     */
    void setOnCreateSocket(Socket::onCreateSocket cb);

    /**
     * 根据socket对象创建Session对象
     * 需要确保在socket归属poller线程执行本函数
     */
    Session::Ptr createSession(const Socket::Ptr &socket);

protected:
    virtual void cloneFrom(const TcpServer &that);
    virtual TcpServer::Ptr onCreatServer(const EventPoller::Ptr &poller);

    virtual Session::Ptr onAcceptConnection(const Socket::Ptr &sock);
    virtual Socket::Ptr onBeforeAcceptConnection(const EventPoller::Ptr &poller);

private:
    void onManagerSession();
    Socket::Ptr createSocket(const EventPoller::Ptr &poller);
    void start_l(uint16_t port, const std::string &host, uint32_t backlog);
    Ptr getServer(const EventPoller *) const;
    void setupEvent();

private:
    bool _is_on_manager = false;
    bool _main_server = true;
    std::weak_ptr<TcpServer> _parent;
    Socket::Ptr _socket;
    std::shared_ptr<Timer> _timer;
    Socket::onCreateSocket _on_create_socket;
    std::unordered_map<SessionHelper *, SessionHelper::Ptr> _session_map;
    std::function<SessionHelper::Ptr(const TcpServer::Ptr &server, const Socket::Ptr &)> _session_alloc;
    std::unordered_map<const EventPoller *, Ptr> _cloned_server;
    //对象个数统计
    ObjectStatistic<TcpServer> _statistic;
};

} /* namespace toolkit */
#endif /* TCPSERVER_TCPSERVER_H */
