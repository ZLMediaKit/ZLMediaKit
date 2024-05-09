/*
 * Copyright (c) 2021 The ZLToolKit project authors. All Rights Reserved.
 *
 * This file is part of ZLToolKit(https://github.com/ZLMediaKit/ZLToolKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#ifndef TOOLKIT_NETWORK_UDPSERVER_H
#define TOOLKIT_NETWORK_UDPSERVER_H

#include "Server.h"
#include "Session.h"

namespace toolkit {

class UdpServer : public Server {
public:
    using Ptr = std::shared_ptr<UdpServer>;
    using PeerIdType = std::string;
    using onCreateSocket = std::function<Socket::Ptr(const EventPoller::Ptr &, const Buffer::Ptr &, struct sockaddr *, int)>;

    explicit UdpServer(const EventPoller::Ptr &poller = nullptr);
    ~UdpServer() override;

    /**
     * @brief 开始监听服务器
     */
    template<typename SessionType>
    void start(uint16_t port, const std::string &host = "::") {
        static std::string cls_name = toolkit::demangle(typeid(SessionType).name());
        // Session 创建器, 通过它创建不同类型的服务器
        _session_alloc = [](const UdpServer::Ptr &server, const Socket::Ptr &sock) {
            auto session = std::shared_ptr<SessionType>(new SessionType(sock), [](SessionType * ptr) {
                TraceP(static_cast<Session *>(ptr)) << "~" << cls_name;
                delete ptr;
            });
            TraceP(static_cast<Session *>(session.get())) << cls_name;
            auto sock_creator = server->_on_create_socket;
            session->setOnCreateSocket([sock_creator](const EventPoller::Ptr &poller) {
                return sock_creator(poller, nullptr, nullptr, 0);
            });
            return std::make_shared<SessionHelper>(server, std::move(session), cls_name);
        };
        start_l(port, host);
    }

    /**
     * @brief 获取服务器监听端口号, 服务器可以选择监听随机端口
     */
    uint16_t getPort();

    /**
     * @brief 自定义socket构建行为
     */
    void setOnCreateSocket(onCreateSocket cb);

protected:
    virtual Ptr onCreatServer(const EventPoller::Ptr &poller);
    virtual void cloneFrom(const UdpServer &that);

private:
    /**
     * @brief 开始udp server
     * @param port 本机端口，0则随机
     * @param host 监听网卡ip
     */
    void start_l(uint16_t port, const std::string &host = "::");

    /**
     * @brief 定时管理 Session, UDP 会话需要根据需要处理超时
     */
    void onManagerSession();

    void onRead(const Buffer::Ptr &buf, struct sockaddr *addr, int addr_len);

    /**
     * @brief 接收到数据,可能来自server fd，也可能来自peer fd
     * @param is_server_fd 时候为server fd
     * @param id 客户端id
     * @param buf 数据
     * @param addr 客户端地址
     * @param addr_len 客户端地址长度
     */
    void onRead_l(bool is_server_fd, const PeerIdType &id, const Buffer::Ptr &buf, struct sockaddr *addr, int addr_len);

    /**
     * @brief 根据对端信息获取或创建一个会话
     */
    SessionHelper::Ptr getOrCreateSession(const PeerIdType &id, const Buffer::Ptr &buf, struct sockaddr *addr, int addr_len, bool &is_new);

    /**
     * @brief 创建一个会话, 同时进行必要的设置
     */
    SessionHelper::Ptr createSession(const PeerIdType &id, const Buffer::Ptr &buf, struct sockaddr *addr, int addr_len);

    /**
     * @brief 创建socket
     */
    Socket::Ptr createSocket(const EventPoller::Ptr &poller, const Buffer::Ptr &buf = nullptr, struct sockaddr *addr = nullptr, int addr_len = 0);

    void setupEvent();

private:
    bool _cloned = false;
    Socket::Ptr _socket;
    std::shared_ptr<Timer> _timer;
    onCreateSocket _on_create_socket;
    //cloned server共享主server的session map，防止数据在不同server间漂移
    std::shared_ptr<std::recursive_mutex> _session_mutex;
    std::shared_ptr<std::unordered_map<PeerIdType, SessionHelper::Ptr> > _session_map;
    //主server持有cloned server的引用
    std::unordered_map<EventPoller *, Ptr> _cloned_server;
    std::function<SessionHelper::Ptr(const UdpServer::Ptr &, const Socket::Ptr &)> _session_alloc;
    // 对象个数统计
    ObjectStatistic<UdpServer> _statistic;
};

} // namespace toolkit

#endif // TOOLKIT_NETWORK_UDPSERVER_H
