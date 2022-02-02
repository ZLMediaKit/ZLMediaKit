/*
 * Copyright (c) 2016 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/xia-chu/ZLMediaKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#ifndef ZLMEDIAKIT_WebSocketClient_H
#define ZLMEDIAKIT_WebSocketClient_H

#include "Util/util.h"
#include "Util/base64.h"
#include "Util/SHA1.h"
#include "Network/TcpClient.h"
#include "HttpClientImp.h"
#include "WebSocketSplitter.h"

namespace mediakit{

template <typename ClientType,WebSocketHeader::Type DataType>
class HttpWsClient;

/**
 * 辅助类,用于拦截TcpClient数据发送前的拦截
 * @tparam ClientType TcpClient派生类
 * @tparam DataType 这里无用,为了声明友元用
 */
template <typename ClientType,WebSocketHeader::Type DataType>
class ClientTypeImp : public ClientType {
public:
    friend class HttpWsClient<ClientType, DataType>;

    using onBeforeSendCB = std::function<ssize_t(const toolkit::Buffer::Ptr &buf)>;

    template<typename ...ArgsType>
    ClientTypeImp(ArgsType &&...args): ClientType(std::forward<ArgsType>(args)...){}
    ~ClientTypeImp() override {};

protected:
    /**
     * 发送前拦截并打包为websocket协议
     */
    ssize_t send(toolkit::Buffer::Ptr buf) override{
        if(_beforeSendCB){
            return _beforeSendCB(buf);
        }
        return ClientType::send(std::move(buf));
    }

    /**
     * 设置发送数据截取回调函数
     * @param cb 截取回调函数
     */
    void setOnBeforeSendCB(const onBeforeSendCB &cb){
        _beforeSendCB = cb;
    }

private:
    onBeforeSendCB _beforeSendCB;
};

/**
 * 此对象完成了weksocket 客户端握手协议，以及到TcpClient派生类事件的桥接
 * @tparam ClientType TcpClient派生类
 * @tparam DataType websocket负载类型，是TEXT还是BINARY类型
 */
template <typename ClientType,WebSocketHeader::Type DataType = WebSocketHeader::TEXT>
class HttpWsClient : public HttpClientImp , public WebSocketSplitter{
public:
    typedef std::shared_ptr<HttpWsClient> Ptr;

    HttpWsClient(const std::shared_ptr<ClientTypeImp<ClientType, DataType> > &delegate) : _weak_delegate(delegate),
                                                                                          _delegate(*delegate) {
        _Sec_WebSocket_Key = encodeBase64(toolkit::makeRandStr(16, false));
        setPoller(_delegate.getPoller());
    }
    ~HttpWsClient(){}

    /**
     * 发起ws握手
     * @param ws_url ws连接url
     * @param fTimeOutSec 超时时间
     */
    void startWsClient(const std::string &ws_url, float fTimeOutSec) {
        std::string http_url = ws_url;
        toolkit::replace(http_url, "ws://", "http://");
        toolkit::replace(http_url, "wss://", "https://");
        setMethod("GET");
        addHeader("Upgrade", "websocket");
        addHeader("Connection", "Upgrade");
        addHeader("Sec-WebSocket-Version", "13");
        addHeader("Sec-WebSocket-Key", _Sec_WebSocket_Key);
        _onRecv = nullptr;
        setHeaderTimeout(fTimeOutSec * 1000);
        sendRequest(http_url);
    }

    void closeWsClient(){
        if(!_onRecv){
            //未连接
            return;
        }
        WebSocketHeader header;
        header._fin = true;
        header._reserved = 0;
        header._opcode = CLOSE;
        //客户端需要加密
        header._mask_flag = true;
        WebSocketSplitter::encode(header, nullptr);
    }

protected:
    //HttpClientImp override

    /**
     * 收到http回复头
     * @param status 状态码，譬如:200 OK
     * @param headers http头
     */
    void onResponseHeader(const std::string &status, const HttpHeader &headers) override {
        if(status == "101"){
            auto Sec_WebSocket_Accept = encodeBase64(toolkit::SHA1::encode_bin(_Sec_WebSocket_Key + "258EAFA5-E914-47DA-95CA-C5AB0DC85B11"));
            if(Sec_WebSocket_Accept == const_cast<HttpHeader &>(headers)["Sec-WebSocket-Accept"]){
                //success
                onWebSocketException(toolkit::SockException());
                //防止ws服务器返回Content-Length
                const_cast<HttpHeader &>(headers).erase("Content-Length");
                return;
            }
            shutdown(toolkit::SockException(toolkit::Err_shutdown, StrPrinter << "Sec-WebSocket-Accept mismatch"));
            return;
        }

        shutdown(toolkit::SockException(toolkit::Err_shutdown,StrPrinter << "bad http status code:" << status));
    };

    /**
     * 接收http回复完毕,
     */
    void onResponseCompleted(const toolkit::SockException &ex) override {}

    /**
     * 接收websocket负载数据
     */
    void onResponseBody(const char *buf,size_t size) override{
        if(_onRecv){
            //完成websocket握手后，拦截websocket数据并解析
            _onRecv(buf, size);
        }
    };

    //TcpClient override

    void onRecv(const toolkit::Buffer::Ptr &buf) override {
        auto strong_ref = _weak_delegate.lock();;
        HttpClientImp::onRecv(buf);
    }

    /**
     * 定时触发
     */
    void onManager() override {
        auto strong_ref = _weak_delegate.lock();;
        if (_onRecv) {
            //websocket连接成功了
            _delegate.onManager();
        } else {
            //websocket连接中...
            HttpClientImp::onManager();
        }
    }

    /**
     * 数据全部发送完毕后回调
     */
    void onFlush() override {
        auto strong_ref = _weak_delegate.lock();;
        if (_onRecv) {
            //websocket连接成功了
            _delegate.onFlush();
        } else {
            //websocket连接中...
            HttpClientImp::onFlush();
        }
    }

    /**
     * tcp连接结果
     */
    void onConnect(const toolkit::SockException &ex) override {
        auto strong_ref = _weak_delegate.lock();;
        if (ex) {
            //tcp连接失败，直接返回失败
            onWebSocketException(ex);
            return;
        }
        //开始websocket握手
        HttpClientImp::onConnect(ex);
    }

    /**
     * tcp连接断开
     */
    void onErr(const toolkit::SockException &ex) override {
        auto strong_ref = _weak_delegate.lock();;
        //tcp断开或者shutdown导致的断开
        onWebSocketException(ex);
    }

    //WebSocketSplitter override

    /**
     * 收到一个webSocket数据包包头，后续将继续触发onWebSocketDecodePayload回调
     * @param header 数据包头
     */
    void onWebSocketDecodeHeader(const WebSocketHeader &header) override{
        _payload_section.clear();
    }

    /**
     * 收到webSocket数据包负载
     * @param header 数据包包头
     * @param ptr 负载数据指针
     * @param len 负载数据长度
     * @param recved 已接收数据长度(包含本次数据长度)，等于header._payload_len时则接受完毕
     */
    void onWebSocketDecodePayload(const WebSocketHeader &header, const uint8_t *ptr, size_t len, size_t recved) override{
        _payload_section.append((char *)ptr,len);
    }

    /**
     * 接收到完整的一个webSocket数据包后回调
     * @param header 数据包包头
     */
    void onWebSocketDecodeComplete(const WebSocketHeader &header_in) override{
        WebSocketHeader& header = const_cast<WebSocketHeader&>(header_in);
        auto  flag = header._mask_flag;
        //websocket客户端发送数据需要加密
        header._mask_flag = true;

        switch (header._opcode){
            case WebSocketHeader::CLOSE:{
                //服务器主动关闭
                WebSocketSplitter::encode(header,nullptr);
                shutdown(toolkit::SockException(toolkit::Err_eof,"websocket server close the connection"));
                break;
            }

            case WebSocketHeader::PING:{
                //心跳包
                header._opcode = WebSocketHeader::PONG;
                WebSocketSplitter::encode(header,std::make_shared<toolkit::BufferString>(std::move(_payload_section)));
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
                    //分片缓存太大，需要清空
                }

                //最后一个包
                if (_payload_cache.empty()) {
                    //这个包是唯一个分片
                    _delegate.onRecv(std::make_shared<WebSocketBuffer>(header._opcode, header._fin, std::move(_payload_section)));
                    break;
                }

                //这个包由多个分片组成
                _payload_cache.append(std::move(_payload_section));
                _delegate.onRecv(std::make_shared<WebSocketBuffer>(header._opcode, header._fin, std::move(_payload_cache)));
                _payload_cache.clear();
                break;
            }

            default: break;
        }
        _payload_section.clear();
        header._mask_flag = flag;
    }

    /**
     * websocket数据编码回调
     * @param ptr 数据指针
     * @param len 数据指针长度
     */
    void onWebSocketEncodeData(toolkit::Buffer::Ptr buffer) override{
        HttpClientImp::send(std::move(buffer));
    }

private:
    void onWebSocketException(const toolkit::SockException &ex){
        if(!ex){
            //websocket握手成功
            //此处截取TcpClient派生类发送的数据并进行websocket协议打包
            std::weak_ptr<HttpWsClient> weakSelf = std::dynamic_pointer_cast<HttpWsClient>(shared_from_this());
            _delegate.setOnBeforeSendCB([weakSelf](const toolkit::Buffer::Ptr &buf){
                auto strongSelf = weakSelf.lock();
                if(strongSelf){
                    WebSocketHeader header;
                    header._fin = true;
                    header._reserved = 0;
                    header._opcode = DataType;
                    //客户端需要加密
                    header._mask_flag = true;
                    strongSelf->WebSocketSplitter::encode(header,buf);
                }
                return buf->size();
            });

            //设置sock，否则shutdown等接口都无效
            _delegate.setSock(HttpClientImp::getSock());
            //触发连接成功事件
            _delegate.onConnect(ex);
            //拦截websocket数据接收
            _onRecv = [this](const char *data, size_t len){
                //解析websocket数据包
                this->WebSocketSplitter::decode((uint8_t *)data, len);
            };
            return;
        }

        //websocket握手失败或者tcp连接失败或者中途断开
        if(_onRecv){
            //握手成功之后的中途断开
            _onRecv = nullptr;
            _delegate.onErr(ex);
            return;
        }

        //websocket握手失败或者tcp连接失败
        _delegate.onConnect(ex);
    }

private:
    std::string _Sec_WebSocket_Key;
    std::function<void(const char *data, size_t len)> _onRecv;
    std::weak_ptr<ClientTypeImp<ClientType, DataType>> _weak_delegate;
    ClientTypeImp<ClientType, DataType> &_delegate;
    std::string _payload_section;
    std::string _payload_cache;
};

/**
 * Tcp客户端转WebSocket客户端模板，
 * 通过该模板，开发者再不修改TcpClient派生类任何代码的情况下快速实现WebSocket协议的包装
 * @tparam ClientType TcpClient派生类
 * @tparam DataType websocket负载类型，是TEXT还是BINARY类型
 * @tparam useWSS 是否使用ws还是wss连接
 */
template <typename ClientType,WebSocketHeader::Type DataType = WebSocketHeader::TEXT,bool useWSS = false >
class WebSocketClient : public ClientTypeImp<ClientType,DataType>{
public:
    typedef std::shared_ptr<WebSocketClient> Ptr;

    template<typename ...ArgsType>
    WebSocketClient(ArgsType &&...args) : ClientTypeImp<ClientType,DataType>(std::forward<ArgsType>(args)...){
    }
    ~WebSocketClient() override {
        _wsClient->closeWsClient();
    }

    /**
     * 重载startConnect方法，
     * 目的是替换TcpClient的连接服务器行为，使之先完成WebSocket握手
     * @param host websocket服务器ip或域名
     * @param iPort websocket服务器端口
     * @param timeout_sec 超时时间
     * @param local_port 本地监听端口，此处不起作用
     */
    void startConnect(const std::string &host, uint16_t port, float timeout_sec = 3, uint16_t local_port = 0) override {
        std::string ws_url;
        if (useWSS) {
            //加密的ws
            ws_url = StrPrinter << "wss://" + host << ":" << port << "/";
        } else {
            //明文ws
            ws_url = StrPrinter << "ws://" + host << ":" << port << "/";
        }
        startWebSocket(ws_url, timeout_sec);
    }

    void startWebSocket(const std::string &ws_url, float fTimeOutSec = 3) {
        _wsClient = std::make_shared<HttpWsClient<ClientType, DataType> >(std::static_pointer_cast<WebSocketClient>(this->shared_from_this()));
        _wsClient->setOnCreateSocket([this](const toolkit::EventPoller::Ptr &){
            return this->createSocket();
        });
        _wsClient->startWsClient(ws_url,fTimeOutSec);
    }

    HttpClient &getHttpClient() {
        return *_wsClient;
    }

private:
    typename HttpWsClient<ClientType,DataType>::Ptr _wsClient;
};

}//namespace mediakit
#endif //ZLMEDIAKIT_WebSocketClient_H
