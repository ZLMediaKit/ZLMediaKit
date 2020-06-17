/*
 * Copyright (c) 2016 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/xiongziliang/ZLMediaKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#ifndef ZLMEDIAKIT_WEBSOCKETSESSION_H
#define ZLMEDIAKIT_WEBSOCKETSESSION_H

#include "HttpSession.h"
#include "Network/TcpServer.h"

/**
 * 数据发送拦截器
 */
class SendInterceptor{
public:
    typedef function<int(const Buffer::Ptr &buf)> onBeforeSendCB;
    SendInterceptor() = default;
    virtual ~SendInterceptor() = default;
    virtual void setOnBeforeSendCB(const onBeforeSendCB &cb) = 0;
};

/**
 * 该类实现了TcpSession派生类发送数据的截取
 * 目的是发送业务数据前进行websocket协议的打包
 */
template <typename TcpSessionType>
class TcpSessionTypeImp : public TcpSessionType, public SendInterceptor{
public:
    typedef std::shared_ptr<TcpSessionTypeImp> Ptr;

    TcpSessionTypeImp(const Parser &header, const HttpSession &parent, const Socket::Ptr &pSock) :
            _identifier(parent.getIdentifier()), TcpSessionType(pSock) {}

    ~TcpSessionTypeImp() {}

    /**
     * 设置发送数据截取回调函数
     * @param cb 截取回调函数
     */
    void setOnBeforeSendCB(const onBeforeSendCB &cb) override {
        _beforeSendCB = cb;
    }

protected:
    /**
     * 重载send函数截取数据
     * @param buf 需要截取的数据
     * @return 数据字节数
     */
    int send(const Buffer::Ptr &buf) override {
        if (_beforeSendCB) {
            return _beforeSendCB(buf);
        }
        return TcpSessionType::send(buf);
    }

    string getIdentifier() const override {
        return _identifier;
    }

private:
    onBeforeSendCB _beforeSendCB;
    string _identifier;
};

template <typename TcpSessionType>
class TcpSessionCreator {
public:
    //返回的TcpSession必须派生于SendInterceptor，可以返回null
    TcpSession::Ptr operator()(const Parser &header, const HttpSession &parent, const Socket::Ptr &pSock){
        return std::make_shared<TcpSessionTypeImp<TcpSessionType> >(header,parent,pSock);
    }
};


/**
* 通过该模板类可以透明化WebSocket协议，
* 用户只要实现WebSock协议下的具体业务协议，譬如基于WebSocket协议的Rtmp协议等
*/
template<typename Creator, typename HttpSessionType = HttpSession, WebSocketHeader::Type DataType = WebSocketHeader::TEXT>
class WebSocketSessionBase : public HttpSessionType {
public:
    WebSocketSessionBase(const Socket::Ptr &pSock) : HttpSessionType(pSock){}
    virtual ~WebSocketSessionBase(){}

    //收到eof或其他导致脱离TcpServer事件的回调
    void onError(const SockException &err) override{
        HttpSessionType::onError(err);
        if(_session){
            _session->onError(err);
        }
    }
    //每隔一段时间触发，用来做超时管理
    void onManager() override{
        if(_session){
            _session->onManager();
        }else{
            HttpSessionType::onManager();
        }
    }

    void attachServer(const TcpServer &server) override{
        HttpSessionType::attachServer(server);
        _weakServer = const_cast<TcpServer &>(server).shared_from_this();
    }
protected:
    /**
     * websocket客户端连接上事件
     * @param header http头
     * @return true代表允许websocket连接，否则拒绝
     */
    bool onWebSocketConnect(const Parser &header) override{
        //创建websocket session类
        _session = _creator(header, *this,HttpSessionType::_sock);
        if(!_session){
            //此url不允许创建websocket连接
            return false;
        }
        auto strongServer = _weakServer.lock();
        if(strongServer){
            _session->attachServer(*strongServer);
        }

        //此处截取数据并进行websocket协议打包
        weak_ptr<WebSocketSessionBase> weakSelf = dynamic_pointer_cast<WebSocketSessionBase>(HttpSessionType::shared_from_this());
        dynamic_pointer_cast<SendInterceptor>(_session)->setOnBeforeSendCB([weakSelf](const Buffer::Ptr &buf) {
            auto strongSelf = weakSelf.lock();
            if (strongSelf) {
                WebSocketHeader header;
                header._fin = true;
                header._reserved = 0;
                header._opcode = DataType;
                header._mask_flag = false;
                strongSelf->WebSocketSplitter::encode(header, buf);
            }
            return buf->size();
        });

        //允许websocket客户端
        return true;
    }
    /**
     * 开始收到一个webSocket数据包
     * @param packet
     */
    void onWebSocketDecodeHeader(const WebSocketHeader &packet) override{
        //新包，原来的包残余数据清空掉
        _remian_data.clear();
    }

    /**
     * 收到websocket数据包负载
     * @param packet
     * @param ptr
     * @param len
     * @param recved
     */
    void onWebSocketDecodePayload(const WebSocketHeader &packet,const uint8_t *ptr,uint64_t len,uint64_t recved) override {
        _remian_data.append((char *)ptr,len);
    }

    /**
     * 接收到完整的一个webSocket数据包后回调
     * @param header 数据包包头
     */
    void onWebSocketDecodeComplete(const WebSocketHeader &header_in) override {
        WebSocketHeader& header = const_cast<WebSocketHeader&>(header_in);
        auto  flag = header._mask_flag;
        header._mask_flag = false;

        switch (header._opcode){
            case WebSocketHeader::CLOSE:{
                HttpSessionType::encode(header,nullptr);
            }
                break;
            case WebSocketHeader::PING:{
                header._opcode = WebSocketHeader::PONG;
                HttpSessionType::encode(header,std::make_shared<BufferString>(_remian_data));
            }
                break;
            case WebSocketHeader::CONTINUATION:{

            }
                break;
            case WebSocketHeader::TEXT:
            case WebSocketHeader::BINARY:{
                _session->onRecv(std::make_shared<BufferString>(_remian_data));
            }
                break;
            default:
                break;
        }
        _remian_data.clear();
        header._mask_flag = flag;
    }

    /**
    * 发送数据进行websocket协议打包后回调
    * @param buffer
    */
    void onWebSocketEncodeData(const Buffer::Ptr &buffer) override{
        HttpSessionType::send(buffer);
    }
private:
    string _remian_data;
    weak_ptr<TcpServer> _weakServer;
    TcpSession::Ptr _session;
    Creator _creator;
};


template<typename TcpSessionType,typename HttpSessionType = HttpSession,WebSocketHeader::Type DataType = WebSocketHeader::TEXT>
class WebSocketSession : public WebSocketSessionBase<TcpSessionCreator<TcpSessionType>,HttpSessionType,DataType>{
public:
    WebSocketSession(const Socket::Ptr &pSock) : WebSocketSessionBase<TcpSessionCreator<TcpSessionType>,HttpSessionType,DataType>(pSock){}
    virtual ~WebSocketSession(){}
};

#endif //ZLMEDIAKIT_WEBSOCKETSESSION_H
