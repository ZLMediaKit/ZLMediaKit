/*
 * Copyright (c) 2016-present The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/ZLMediaKit/ZLMediaKit).
 *
 * Use of this source code is governed by MIT-like license that can be found in the
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

namespace mediakit {

template <typename ClientType, WebSocketHeader::Type DataType>
class HttpWsClient;

/**
 * 辅助类,用于拦截TcpClient数据发送前的拦截
 * @tparam ClientType TcpClient派生类
 * @tparam DataType 这里无用,为了声明友元用
 * Helper class for intercepting data sent by TcpClient before sending
 * @tparam ClientType TcpClient derived class
 * @tparam DataType This is useless, used for declaring friends
 
 * [AUTO-TRANSLATED:02cc7424]
 */
template <typename ClientType, WebSocketHeader::Type DataType>
class ClientTypeImp : public ClientType {
public:
    friend class HttpWsClient<ClientType, DataType>;

    using onBeforeSendCB = std::function<ssize_t(const toolkit::Buffer::Ptr &buf)>;

    template <typename... ArgsType>
    ClientTypeImp(ArgsType &&...args) : ClientType(std::forward<ArgsType>(args)...) {}

    /**
     * 发送前拦截并打包为websocket协议
     * Intercept before sending and package it into websocket protocol
     
     * [AUTO-TRANSLATED:b43b6169]
     */
    ssize_t send(toolkit::Buffer::Ptr buf) override {
        if (_beforeSendCB) {
            return _beforeSendCB(buf);
        }
        return ClientType::send(std::move(buf));
    }

protected:
    /**
     * 设置发送数据截取回调函数
     * @param cb 截取回调函数
     * Set the data interception callback function
     * @param cb Interception callback function
     
     * [AUTO-TRANSLATED:3e74fcdd]
     */
    void setOnBeforeSendCB(const onBeforeSendCB &cb) { _beforeSendCB = cb; }

private:
    onBeforeSendCB _beforeSendCB;
};

/**
 * 此对象完成了weksocket 客户端握手协议，以及到TcpClient派生类事件的桥接
 * @tparam ClientType TcpClient派生类
 * @tparam DataType websocket负载类型，是TEXT还是BINARY类型
 * This object completes the weksocket client handshake protocol and bridges to the TcpClient derived class events
 * @tparam ClientType TcpClient derived class
 * @tparam DataType websocket payload type, TEXT or BINARY type
 
 * [AUTO-TRANSLATED:912c15f6]
 */
template <typename ClientType, WebSocketHeader::Type DataType = WebSocketHeader::TEXT>
class HttpWsClient : public HttpClientImp, public WebSocketSplitter {
public:
    using Ptr = std::shared_ptr<HttpWsClient>;

    HttpWsClient(const std::shared_ptr<ClientTypeImp<ClientType, DataType>> &delegate) : _weak_delegate(delegate) {
        _Sec_WebSocket_Key = encodeBase64(toolkit::makeRandStr(16, false));
        setPoller(delegate->getPoller());
    }

    /**
     * 发起ws握手
     * @param ws_url ws连接url
     * @param fTimeOutSec 超时时间
     * Initiate ws handshake
     * @param ws_url ws connection url
     * @param fTimeOutSec Timeout time
     
     * [AUTO-TRANSLATED:453c027c]
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

    void closeWsClient() {
        if (!_onRecv) {
            // 未连接  [AUTO-TRANSLATED:94510177]
            // Not connected
            return;
        }
        WebSocketHeader header;
        header._fin = true;
        header._reserved = 0;
        header._opcode = CLOSE;
        // 客户端需要加密  [AUTO-TRANSLATED:d6958acf]
        // Client needs encryption
        header._mask_flag = true;
        WebSocketSplitter::encode(header, nullptr);
    }

protected:
    // HttpClientImp override

    /**
     * 收到http回复头
     * @param status 状态码，譬如:200 OK
     * @param headers http头
     * Receive http response header
     * @param status Status code, such as: 200 OK
     * @param headers http header
     
     * [AUTO-TRANSLATED:a685f8ef]
     */
    void onResponseHeader(const std::string &status, const HttpHeader &headers) override {
        if (status == "101") {
            auto Sec_WebSocket_Accept = encodeBase64(toolkit::SHA1::encode_bin(_Sec_WebSocket_Key + "258EAFA5-E914-47DA-95CA-C5AB0DC85B11"));
            if (Sec_WebSocket_Accept == const_cast<HttpHeader &>(headers)["Sec-WebSocket-Accept"]) {
                // success
                onWebSocketException(toolkit::SockException());
                // 防止ws服务器返回Content-Length  [AUTO-TRANSLATED:f4454ae6]
                // Prevent ws server from returning Content-Length
                const_cast<HttpHeader &>(headers).erase("Content-Length");
                return;
            }
            shutdown(toolkit::SockException(toolkit::Err_shutdown, StrPrinter << "Sec-WebSocket-Accept mismatch"));
            return;
        }

        shutdown(toolkit::SockException(toolkit::Err_shutdown, StrPrinter << "bad http status code:" << status));
    };

    /**
     * 接收http回复完毕,
     * Receive http response complete,
     
     * [AUTO-TRANSLATED:b96ed715]
     */
    void onResponseCompleted(const toolkit::SockException &ex) override {}

    /**
     * 接收websocket负载数据
     * Receive websocket payload data
     
     * [AUTO-TRANSLATED:55d403d9]
     */
    void onResponseBody(const char *buf, size_t size) override {
        if (_onRecv) {
            // 完成websocket握手后，拦截websocket数据并解析  [AUTO-TRANSLATED:734280fe]
            // After completing the websocket handshake, intercept the websocket data and parse it
            _onRecv(buf, size);
        }
    };

    // TcpClient override

    void onRecv(const toolkit::Buffer::Ptr &buf) override {
        HttpClientImp::onRecv(buf);
    }

    /**
     * 定时触发
     * Triggered periodically
     
     * [AUTO-TRANSLATED:2a75dbf6]
     */
    void onManager() override {
        if (_onRecv) {
            // websocket连接成功了  [AUTO-TRANSLATED:45a9e005]
            // websocket connection succeeded
            if (auto strong_ref = _weak_delegate.lock()) {
                strong_ref->onManager();
            }
        } else {
            // websocket连接中...  [AUTO-TRANSLATED:861cb158]
            // websocket connecting...
            HttpClientImp::onManager();
        }

        if (!_onRecv) {
            // websocket尚未链接  [AUTO-TRANSLATED:164129da]
            // websocket not yet connected
            return;
        }

        if (_recv_ticker.elapsedTime() > 30 * 1000) {
            shutdown(toolkit::SockException(toolkit::Err_timeout, "websocket timeout"));
        } else if (_recv_ticker.elapsedTime() > 10 * 1000) {
            // 没收到回复，每10秒发送次ping 包  [AUTO-TRANSLATED:31b4dc13]
            // No response received, send a ping packet every 10 seconds
            WebSocketHeader header;
            header._fin = true;
            header._reserved = 0;
            header._opcode = PING;
            header._mask_flag = true;
            WebSocketSplitter::encode(header, nullptr);
        }
    }

    /**
     * 数据全部发送完毕后回调
     * Callback after all data has been sent
     
     * [AUTO-TRANSLATED:8b2ba800]
     */
    void onFlush() override {
        if (_onRecv) {
            // websocket连接成功了  [AUTO-TRANSLATED:45a9e005]
            // websocket connection succeeded
            if (auto strong_ref = _weak_delegate.lock()) {
                strong_ref->onFlush();
            }
        } else {
            // websocket连接中...  [AUTO-TRANSLATED:861cb158]
            // websocket connecting...
            HttpClientImp::onFlush();
        }
    }

    /**
     * tcp连接结果
     * tcp connection result
     
     * [AUTO-TRANSLATED:eaca9fcc]
     */
    void onConnect(const toolkit::SockException &ex) override {
        if (ex) {
            // tcp连接失败，直接返回失败  [AUTO-TRANSLATED:dcd81b67]
            // tcp connection failed, return failure directly
            onWebSocketException(ex);
            return;
        }
        // 开始websocket握手  [AUTO-TRANSLATED:544a5ba3]
        // Start websocket handshake
        HttpClientImp::onConnect(ex);
    }

    /**
     * tcp连接断开
     * tcp connection disconnected
     
     * [AUTO-TRANSLATED:732b0740]
     */
    void onError(const toolkit::SockException &ex) override {
        // tcp断开或者shutdown导致的断开  [AUTO-TRANSLATED:5b6b7ad4]
        // Disconnection caused by tcp disconnection or shutdown
        onWebSocketException(ex);
    }

    // WebSocketSplitter override

    /**
     * 收到一个webSocket数据包包头，后续将继续触发onWebSocketDecodePayload回调
     * @param header 数据包头
     * Receive a webSocket data packet header, and then continue to trigger the onWebSocketDecodePayload callback
     * @param header Data packet header
     
     * [AUTO-TRANSLATED:7bc6b7c6]
     */
    void onWebSocketDecodeHeader(const WebSocketHeader &header) override { _payload_section.clear(); }

    /**
     * 收到webSocket数据包负载
     * @param header 数据包包头
     * @param ptr 负载数据指针
     * @param len 负载数据长度
     * @param recved 已接收数据长度(包含本次数据长度)，等于header._payload_len时则接受完毕
     * Receive webSocket data packet payload
     * @param header Data packet header
     * @param ptr Payload data pointer
     * @param len Payload data length
     * @param recved Received data length (including the current data length), equal to header._payload_len when the reception is complete
     
     * [AUTO-TRANSLATED:ca056d2e]
     */
    void onWebSocketDecodePayload(const WebSocketHeader &header, const uint8_t *ptr, size_t len, size_t recved) override {
        _payload_section.append((char *)ptr, len);
    }

    /**
     * 接收到完整的一个webSocket数据包后回调
     * @param header 数据包包头
     * Callback after receiving a complete webSocket data packet
     * @param header Data packet header
     
     * [AUTO-TRANSLATED:f506a7c5]
     */
    void onWebSocketDecodeComplete(const WebSocketHeader &header_in) override {
        WebSocketHeader &header = const_cast<WebSocketHeader &>(header_in);
        auto flag = header._mask_flag;
        // websocket客户端发送数据需要加密  [AUTO-TRANSLATED:2bbbb390]
        // websocket client needs to encrypt data sent
        header._mask_flag = true;
        _recv_ticker.resetTime();
        switch (header._opcode) {
            case WebSocketHeader::CLOSE: {
                // 服务器主动关闭  [AUTO-TRANSLATED:5a59e1bf]
                // Server actively closes
                WebSocketSplitter::encode(header, nullptr);
                shutdown(toolkit::SockException(toolkit::Err_eof, "websocket server close the connection"));
                break;
            }

            case WebSocketHeader::PING: {
                // 心跳包  [AUTO-TRANSLATED:1b4b9ae4]
                // Heartbeat packet
                header._opcode = WebSocketHeader::PONG;
                WebSocketSplitter::encode(header, std::make_shared<toolkit::BufferString>(std::move(_payload_section)));
                break;
            }

            case WebSocketHeader::CONTINUATION:
            case WebSocketHeader::TEXT:
            case WebSocketHeader::BINARY: {
                if (!header._fin) {
                    // 还有后续分片数据, 我们先缓存数据，所有分片收集完成才一次性输出  [AUTO-TRANSLATED:0a237b29]
                    // There are subsequent fragment data, we cache the data first, and output it all at once after all fragments are collected
                    _payload_cache.append(std::move(_payload_section));
                    if (_payload_cache.size() < MAX_WS_PACKET) {
                        // 还有内存容量缓存分片数据  [AUTO-TRANSLATED:8da8074a]
                        // There is also memory capacity to cache fragment data
                        break;
                    }
                    // 分片缓存太大，需要清空  [AUTO-TRANSLATED:a0d9c101]
                    // Fragment cache is too large, need to clear
                }

                // 最后一个包  [AUTO-TRANSLATED:82e1bf79]
                // Last packet
                if (_payload_cache.empty()) {
                    // 这个包是唯一个分片  [AUTO-TRANSLATED:079a9865]
                    // This packet is the only fragment
                    if (auto strong_ref = _weak_delegate.lock()) {
                        strong_ref->onRecv(std::make_shared<WebSocketBuffer>(header._opcode, header._fin, std::move(_payload_section)));
                    }
                    break;
                }

                // 这个包由多个分片组成  [AUTO-TRANSLATED:27fd75df]
                // This packet consists of multiple fragments
                _payload_cache.append(std::move(_payload_section));
                if (auto strong_ref = _weak_delegate.lock()) {
                    strong_ref->onRecv(std::make_shared<WebSocketBuffer>(header._opcode, header._fin, std::move(_payload_cache)));
                }
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
     * websocket data encoding callback
     * @param ptr data pointer
     * @param len data pointer length
     
     * [AUTO-TRANSLATED:7c940c67]
     */
    void onWebSocketEncodeData(toolkit::Buffer::Ptr buffer) override { HttpClientImp::send(std::move(buffer)); }

private:
    void onWebSocketException(const toolkit::SockException &ex) {
        if (!ex) {
            // websocket握手成功  [AUTO-TRANSLATED:bceba441]
            // websocket handshake successful
            // 此处截取TcpClient派生类发送的数据并进行websocket协议打包  [AUTO-TRANSLATED:8cae42cd]
            // Here, the data sent by the TcpClient derived class is intercepted and packaged into the websocket protocol
            std::weak_ptr<HttpWsClient> weakSelf = std::static_pointer_cast<HttpWsClient>(shared_from_this());
            if (auto strong_ref = _weak_delegate.lock()) {
                strong_ref->setOnBeforeSendCB([weakSelf](const toolkit::Buffer::Ptr &buf) {
                    auto strong_self = weakSelf.lock();
                    if (strong_self) {
                        WebSocketHeader header;
                        header._fin = true;
                        header._reserved = 0;
                        header._opcode = DataType;
                        // 客户端需要加密  [AUTO-TRANSLATED:d6958acf]
                        // Client needs encryption
                        header._mask_flag = true;
                        strong_self->WebSocketSplitter::encode(header, buf);
                    }
                    return buf->size();
                });
                // 设置sock，否则shutdown等接口都无效  [AUTO-TRANSLATED:4586b98b]
                // Set sock, otherwise shutdown and other interfaces are invalid
                strong_ref->setSock(HttpClientImp::getSock());
                // 触发连接成功事件  [AUTO-TRANSLATED:0459f68f]
                // Trigger connection success event
                strong_ref->onConnect(ex);
            }

            // 拦截websocket数据接收  [AUTO-TRANSLATED:fb93bbe9]
            // Intercept websocket data reception
            _onRecv = [this](const char *data, size_t len) {
                // 解析websocket数据包  [AUTO-TRANSLATED:656b8c89]
                // Parse websocket data packet
                this->WebSocketSplitter::decode((uint8_t *)data, len);
            };
            return;
        }

        // websocket握手失败或者tcp连接失败或者中途断开  [AUTO-TRANSLATED:acf8d1ff]
        // websocket handshake failed or tcp connection failed or disconnected in the middle
        if (_onRecv) {
            // 握手成功之后的中途断开  [AUTO-TRANSLATED:dd5d412c]
            // Disconnected in the middle after handshake success
            _onRecv = nullptr;
            if (auto strong_ref = _weak_delegate.lock()) {
                strong_ref->onError(ex);
            }
            return;
        }

        // websocket握手失败或者tcp连接失败  [AUTO-TRANSLATED:3f03cf1f]
        // websocket handshake failed or tcp connection failed
        if (auto strong_ref = _weak_delegate.lock()) {
            strong_ref->onConnect(ex);
        }
    }

private:
    std::string _Sec_WebSocket_Key;
    std::function<void(const char *data, size_t len)> _onRecv;
    std::weak_ptr<ClientTypeImp<ClientType, DataType>> _weak_delegate;
    std::string _payload_section;
    std::string _payload_cache;
    toolkit::Ticker _recv_ticker;
};

/**
 * Tcp客户端转WebSocket客户端模板，
 * 通过该模板，开发者再不修改TcpClient派生类任何代码的情况下快速实现WebSocket协议的包装
 * @tparam ClientType TcpClient派生类
 * @tparam DataType websocket负载类型，是TEXT还是BINARY类型
 * @tparam useWSS 是否使用ws还是wss连接
 * Tcp client to WebSocket client template,
 * Through this template, developers can quickly implement WebSocket protocol packaging without modifying any code of the TcpClient derived class
 * @tparam ClientType TcpClient derived class
 * @tparam DataType websocket payload type, is it TEXT or BINARY type
 * @tparam useWSS Whether to use ws or wss connection
 
 * [AUTO-TRANSLATED:ac1516b8]
 */
template <typename ClientType, WebSocketHeader::Type DataType = WebSocketHeader::TEXT, bool useWSS = false>
class WebSocketClient : public ClientTypeImp<ClientType, DataType> {
public:
    using Ptr = std::shared_ptr<WebSocketClient>;

    template <typename... ArgsType>
    WebSocketClient(ArgsType &&...args) : ClientTypeImp<ClientType, DataType>(std::forward<ArgsType>(args)...) {}
    ~WebSocketClient() override { _wsClient->closeWsClient(); }

    /**
     * 重载startConnect方法，
     * 目的是替换TcpClient的连接服务器行为，使之先完成WebSocket握手
     * @param host websocket服务器ip或域名
     * @param iPort websocket服务器端口
     * @param timeout_sec 超时时间
     * @param local_port 本地监听端口，此处不起作用
     * Overload the startConnect method,
     * The purpose is to replace the TcpClient's connection server behavior, so that it completes the WebSocket handshake first
     * @param host websocket server ip or domain name
     * @param iPort websocket server port
     * @param timeout_sec timeout time
     * @param local_port local listening port, which does not work here
     
     * [AUTO-TRANSLATED:1aed295d]
     */
    void startConnect(const std::string &host, uint16_t port, float timeout_sec = 3, uint16_t local_port = 0) override {
        std::string ws_url;
        if (useWSS) {
            // 加密的ws  [AUTO-TRANSLATED:d1385825]
            // Encrypted ws
            ws_url = StrPrinter << "wss://" + host << ":" << port << "/";
        } else {
            // 明文ws  [AUTO-TRANSLATED:71aa82d1]
            // Plaintext ws
            ws_url = StrPrinter << "ws://" + host << ":" << port << "/";
        }
        startWebSocket(ws_url, timeout_sec);
    }

    void startWebSocket(const std::string &ws_url, float fTimeOutSec = 3) {
        _wsClient = std::make_shared<HttpWsClient<ClientType, DataType>>(std::static_pointer_cast<WebSocketClient>(this->shared_from_this()));
        _wsClient->setOnCreateSocket([this](const toolkit::EventPoller::Ptr &) { return this->createSocket(); });
        _wsClient->startWsClient(ws_url, fTimeOutSec);
    }

    HttpClient &getHttpClient() { return *_wsClient; }

private:
    typename HttpWsClient<ClientType, DataType>::Ptr _wsClient;
};

} // namespace mediakit
#endif // ZLMEDIAKIT_WebSocketClient_H
