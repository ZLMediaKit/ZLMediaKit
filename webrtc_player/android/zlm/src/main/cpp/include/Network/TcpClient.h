/*
 * Copyright (c) 2016 The ZLToolKit project authors. All Rights Reserved.
 *
 * This file is part of ZLToolKit(https://github.com/ZLMediaKit/ZLToolKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#ifndef NETWORK_TCPCLIENT_H
#define NETWORK_TCPCLIENT_H

#include <memory>
#include "Socket.h"
#include "Util/SSLBox.h"

namespace toolkit {

//Tcp客户端，Socket对象默认开始互斥锁
class TcpClient : public SocketHelper {
public:
    using Ptr = std::shared_ptr<TcpClient>;
    TcpClient(const EventPoller::Ptr &poller = nullptr);
    ~TcpClient() override;

    /**
     * 开始连接tcp服务器
     * @param url 服务器ip或域名
     * @param port 服务器端口
     * @param timeout_sec 超时时间,单位秒
     * @param local_port 本地端口
     */
    virtual void startConnect(const std::string &url, uint16_t port, float timeout_sec = 5, uint16_t local_port = 0);
    
    /**
     * 通过代理开始连接tcp服务器
     * @param url 服务器ip或域名
     * @proxy_host 代理ip
     * @proxy_port 代理端口
     * @param timeout_sec 超时时间,单位秒
     * @param local_port 本地端口
     */
    virtual void startConnectWithProxy(const std::string &url, const std::string &proxy_host, uint16_t proxy_port, float timeout_sec = 5, uint16_t local_port = 0){};
    
    /**
     * 主动断开连接
     * @param ex 触发onErr事件时的参数
     */
    void shutdown(const SockException &ex = SockException(Err_shutdown, "self shutdown")) override;

    /**
     * 连接中或已连接返回true，断开连接时返回false
     */
    virtual bool alive() const;

    /**
     * 设置网卡适配器,使用该网卡与服务器通信
     * @param local_ip 本地网卡ip
     */
    virtual void setNetAdapter(const std::string &local_ip);

    /**
     * 唯一标识
     */
    std::string getIdentifier() const override;

protected:
    /**
     * 连接服务器结果回调
     * @param ex 成功与否
     */
    virtual void onConnect(const SockException &ex) = 0;

    /**
     * tcp连接成功后每2秒触发一次该事件
     */
    void onManager() override {}

private:
    void onSockConnect(const SockException &ex);

private:
    mutable std::string _id;
    std::string _net_adapter = "::";
    std::shared_ptr<Timer> _timer;
    //对象个数统计
    ObjectStatistic<TcpClient> _statistic;
};

//用于实现TLS客户端的模板对象
template<typename TcpClientType>
class TcpClientWithSSL : public TcpClientType {
public:
    using Ptr = std::shared_ptr<TcpClientWithSSL>;

    template<typename ...ArgsType>
    TcpClientWithSSL(ArgsType &&...args):TcpClientType(std::forward<ArgsType>(args)...) {}

    ~TcpClientWithSSL() override {
        if (_ssl_box) {
            _ssl_box->flush();
        }
    }

    void onRecv(const Buffer::Ptr &buf) override {
        if (_ssl_box) {
            _ssl_box->onRecv(buf);
        } else {
            TcpClientType::onRecv(buf);
        }
    }

    // 使能其他未被重写的send函数
    using TcpClientType::send;

    ssize_t send(Buffer::Ptr buf) override {
        if (_ssl_box) {
            auto size = buf->size();
            _ssl_box->onSend(std::move(buf));
            return size;
        }
        return TcpClientType::send(std::move(buf));
    }

    //添加public_onRecv和public_send函数是解决较低版本gcc一个lambad中不能访问protected或private方法的bug
    inline void public_onRecv(const Buffer::Ptr &buf) {
        TcpClientType::onRecv(buf);
    }

    inline void public_send(const Buffer::Ptr &buf) {
        TcpClientType::send(buf);
    }

    void startConnect(const std::string &url, uint16_t port, float timeout_sec = 5, uint16_t local_port = 0) override {
        _host = url;
        TcpClientType::startConnect(url, port, timeout_sec, local_port);
    }
    void startConnectWithProxy(const std::string &url, const std::string &proxy_host, uint16_t proxy_port, float timeout_sec = 5, uint16_t local_port = 0) override {
        _host = url;
        TcpClientType::startConnect(proxy_host, proxy_port, timeout_sec, local_port);
    }

    bool overSsl() const override { return (bool)_ssl_box; }

protected:
    void onConnect(const SockException &ex) override {
        if (!ex) {
            _ssl_box = std::make_shared<SSL_Box>(false);
            _ssl_box->setOnDecData([this](const Buffer::Ptr &buf) {
                public_onRecv(buf);
            });
            _ssl_box->setOnEncData([this](const Buffer::Ptr &buf) {
                public_send(buf);
            });

            if (!isIP(_host.data())) {
                //设置ssl域名
                _ssl_box->setHost(_host.data());
            }
        }
        TcpClientType::onConnect(ex);
    }
    /**
     * 重置ssl, 主要为了解决一些302跳转时http与https的转换
     */
    void setDoNotUseSSL() {
        _ssl_box.reset();
    }
private:
    std::string _host;
    std::shared_ptr<SSL_Box> _ssl_box;
};

} /* namespace toolkit */
#endif /* NETWORK_TCPCLIENT_H */
