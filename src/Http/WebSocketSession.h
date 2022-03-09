/*
 * Copyright (c) 2016 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/xia-chu/ZLMediaKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#ifndef ZLMEDIAKIT_WEBSOCKETSESSION_H
#define ZLMEDIAKIT_WEBSOCKETSESSION_H

#include "HttpSession.h"
#include "Network/TcpServer.h"
#include "WebSocketSplitter.h"

namespace mediakit {
/**
 * 该类实现TcpSession派生类数据发送的拦截
 * 目的是在发送业务数据前，进行websocket协议打包
 */
template <typename TcpSessionType>
class TcpSessionTypeImp : public SendInterceptor<TcpSessionType> {
    std::string _identifier;
public:
    typedef std::shared_ptr<TcpSessionTypeImp> Ptr;

    TcpSessionTypeImp(const Parser &header, const HttpSession &parent, const toolkit::Socket::Ptr &pSock) :
        SendInterceptor<TcpSessionType>(pSock), _identifier(parent.getIdentifier()) {}
    ~TcpSessionTypeImp() {}

    std::string getIdentifier() const override {
        return _identifier;
    }
};

template <typename TcpSessionType>
class TcpSessionCreator {
public:
    //返回的TcpSession必须派生于SendInterceptor，可以返回null
    toolkit::TcpSession::Ptr operator()(const Parser &header, const HttpSession &parent, const toolkit::Socket::Ptr &pSock){
        return std::make_shared<TcpSessionTypeImp<TcpSessionType> >(header, parent, pSock);
    }
};

/**
* 通过该模板类可以透明化WebSocket协议，
* 用户只要实现WebSock协议下的具体业务协议，譬如基于WebSocket协议的Rtmp协议等
*/
template<typename Creator, typename HttpSessionType = HttpSession>
class WebSocketSessionBase : public HttpSessionType {
public:
    WebSocketSessionBase(const toolkit::Socket::Ptr &pSock) : HttpSessionType(pSock){}
    virtual ~WebSocketSessionBase(){}

    //收到eof或其他导致脱离TcpServer事件的回调
    void onError(const toolkit::SockException &err) override{
        HttpSessionType::onError(err);
        if(_session){
            _session->onError(err);
        }
    }
    //每隔一段时间触发，用来做超时管理
    void onManager() override{
        if(_session)
            _session->onManager();
        else
            HttpSessionType::onManager();
    }

    void attachServer(const toolkit::Server &server) override{
        HttpSessionType::attachServer(server);
        _weak_server = const_cast<toolkit::Server &>(server).shared_from_this();
    }

protected:
    /**
     * websocket客户端连接上事件
     * @param header http头
     * @return true代表允许websocket连接，否则拒绝
     */
    bool onWebSocketConnect(const Parser &header) override{
        //创建websocket session类
        _session = _creator(header, *this, HttpSessionType::getSock());
        if(!_session){
            //此url不允许创建websocket连接
            return false;
        }

        if(auto strongServer = _weak_server.lock()){
            _session->attachServer(*strongServer);
        }

        //此处截取数据并进行websocket协议打包
        std::weak_ptr<WebSocketSessionBase> weakSelf = std::dynamic_pointer_cast<WebSocketSessionBase>(HttpSessionType::shared_from_this());
        std::dynamic_pointer_cast<ISendInterceptor>(_session)->setOnBeforeSendCB([weakSelf](const toolkit::Buffer::Ptr &buf) {
            auto strongSelf = weakSelf.lock();
            if (strongSelf) {
                WebSocketHeader header;
                header._fin = true;
                header._reserved = 0;
                // 建议在SockUtils上增加一个sendBin的方法，生成一个WebSocketBuffer对象
                WebSocketHeader::Type op = WebSocketHeader::TEXT;
                if (auto wbuf = std::dynamic_pointer_cast<WebSocketBuffer>(buf))
                    op = wbuf->headType();
                header._opcode = op;
                header._mask_flag = false;
                strongSelf->HttpSessionType::encode(header, buf);
            }
            return buf->size();
        });

        //允许websocket客户端
        return true;
    }

    /**
     * 开始收到一个webSocket数据包
     */
    void onWebSocketDecodeHeader(const WebSocketHeader &packet) override{
        //新包，原来的包残余数据清空掉
        _payload_section.clear();
    }

    /**
     * 收到websocket数据包负载
     */
    void onWebSocketDecodePayload(const WebSocketHeader &packet,const uint8_t *ptr,size_t len,size_t recved) override {
        _payload_section.append((char *)ptr,len);
    }

    /**
     * 接收到完整的一个webSocket数据包后回调
     * @param header 数据包包头
     */
    void onWebSocketDecodeComplete(const WebSocketHeader &header_in) override {
        auto header = const_cast<WebSocketHeader&>(header_in);
        auto flag = header._mask_flag;
        header._mask_flag = false;

        switch (header._opcode){
            case WebSocketHeader::CLOSE:{
                HttpSessionType::encode(header,nullptr);
                HttpSessionType::shutdown(toolkit::SockException(toolkit::Err_shutdown, "recv close request from client"));
                break;
            }
            
            case WebSocketHeader::PING:{
                header._opcode = WebSocketHeader::PONG;
                HttpSessionType::encode(header, std::make_shared<toolkit::BufferString>(_payload_section));
                break;
            }
            
            case WebSocketHeader::CONTINUATION:
            case WebSocketHeader::TEXT:
            case WebSocketHeader::BINARY:{
                if (!header._fin) {
                    //还有后续分片数据, 我们先缓存数据，所有分片收集完成才一次性输出
                    _payload_cache.append(std::move(_payload_section));
                    if (_payload_cache.size() < MAX_WS_PACKET) {
                        //还有内存容量缓存分片数据
                        break;
                    }
                    //分片缓存太大，需要清空，这里会分成多个切片返回，并由fin来看是否结束
                }

                if (_payload_cache.empty()) {
                    //这个包是唯一个分片
                    _session->onRecv(std::make_shared<WebSocketBuffer>(header._opcode, header._fin, std::move(_payload_section)));
                }
                else {
                    //这个包由多个分片组成
                    _payload_cache.append(std::move(_payload_section));
                    _session->onRecv(std::make_shared<WebSocketBuffer>(header._opcode, header._fin, std::move(_payload_cache)));
                    _payload_cache.clear();
                }
                break;
            }
            
            default: 
                break;
        }
        _payload_section.clear();
        header._mask_flag = flag;
    }

    /**
     * 发送数据进行websocket协议打包后回调
    */
    void onWebSocketEncodeData(toolkit::Buffer::Ptr buffer) override{
        HttpSessionType::send(std::move(buffer));
    }

private:
    // websocket分片缓存，用于形成一个websocket frame
    std::string _payload_cache;
    // 当前websocket分片
    std::string _payload_section;
    // 用于attach server
    std::weak_ptr<toolkit::Server> _weak_server;
    // TcpSessionTypeImp类型
    toolkit::TcpSession::Ptr _session;
    // TcpSessionTypeImp创建器
    Creator _creator;
};


template<typename TcpSessionType, typename HttpSessionType = HttpSession>
class WebSocketSession : public WebSocketSessionBase<TcpSessionCreator<TcpSessionType>, HttpSessionType>{
public:
    WebSocketSession(const toolkit::Socket::Ptr &pSock) : WebSocketSessionBase<TcpSessionCreator<TcpSessionType>, HttpSessionType>(pSock){}
    virtual ~WebSocketSession(){}
};

}
#endif //ZLMEDIAKIT_WEBSOCKETSESSION_H
