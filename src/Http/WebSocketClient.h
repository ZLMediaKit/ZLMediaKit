/*
 * MIT License
 *
 * Copyright (c) 2016-2019 xiongziliang <771730766@qq.com>
 *
 * This file is part of ZLMediaKit(https://github.com/xiongziliang/ZLMediaKit).
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#ifndef ZLMEDIAKIT_WebSocketClient_H
#define ZLMEDIAKIT_WebSocketClient_H

#include "Util/util.h"
#include "Util/base64.h"
#include "Util/SHA1.h"
#include "Network/TcpClient.h"
#include "HttpClientImp.h"
#include "WebSocketSplitter.h"
using namespace toolkit;

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
    typedef function<int(const Buffer::Ptr &buf)> onBeforeSendCB;
    friend class HttpWsClient<ClientType,DataType>;

    template<typename ...ArgsType>
    ClientTypeImp(ArgsType &&...args): ClientType(std::forward<ArgsType>(args)...){}
    ~ClientTypeImp() override {};
protected:
    /**
     * 发送前拦截并打包为websocket协议
     * @param buf
     * @return
     */
    int send(const Buffer::Ptr &buf) override{
        if(_beforeSendCB){
            return _beforeSendCB(buf);
        }
        return ClientType::send(buf);
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
    typedef shared_ptr<HttpWsClient> Ptr;

    HttpWsClient(ClientTypeImp<ClientType,DataType> &delegate) : _delegate(delegate){
        _Sec_WebSocket_Key = encodeBase64(SHA1::encode_bin(makeRandStr(16, false)));
        setPoller(delegate.getPoller());
    }
    ~HttpWsClient(){}

    /**
     * 发起ws握手
     * @param ws_url ws连接url
     * @param fTimeOutSec 超时时间
     */
    void startWsClient(const string &ws_url,float fTimeOutSec){
        string http_url = ws_url;
        replace(http_url,"ws://","http://");
        replace(http_url,"wss://","https://");
        setMethod("GET");
        addHeader("Upgrade","websocket");
        addHeader("Connection","Upgrade");
        addHeader("Sec-WebSocket-Version","13");
        addHeader("Sec-WebSocket-Key",_Sec_WebSocket_Key);
        _onRecv = nullptr;
        sendRequest(http_url,fTimeOutSec);
    }
protected:
    //HttpClientImp override

    /**
     * 收到http回复头
     * @param status 状态码，譬如:200 OK
     * @param headers http头
     * @return 返回后续content的长度；-1:后续数据全是content；>=0:固定长度content
     *          需要指出的是，在http头中带有Content-Length字段时，该返回值无效
     */
    int64_t onResponseHeader(const string &status,const HttpHeader &headers) override {
        if(status == "101"){
            auto Sec_WebSocket_Accept = encodeBase64(SHA1::encode_bin(_Sec_WebSocket_Key + "258EAFA5-E914-47DA-95CA-C5AB0DC85B11"));
            if(Sec_WebSocket_Accept == const_cast<HttpHeader &>(headers)["Sec-WebSocket-Accept"]){
                //success
                onWebSocketException(SockException());
                return 0;
            }
            shutdown(SockException(Err_shutdown,StrPrinter << "Sec-WebSocket-Accept mismatch"));
            return 0;
        }

        shutdown(SockException(Err_shutdown,StrPrinter << "bad http status code:" << status));
        return 0;
    };

    /**
     * 接收http回复完毕,
     */
    void onResponseCompleted() override {}

    //TcpClient override

    /**
     * 定时触发
     */
    void onManager() override {
        if(_onRecv){
            //websocket连接成功了
            _delegate.onManager();
        } else{
            //websocket连接中...
            HttpClientImp::onManager();
        }
    }

    /**
     * 数据全部发送完毕后回调
     */
    void onFlush() override{
        if(_onRecv){
            //websocket连接成功了
            _delegate.onFlush();
        } else{
            //websocket连接中...
            HttpClientImp::onFlush();
        }
    }

    /**
     * tcp连接结果
     * @param ex
     */
    void onConnect(const SockException &ex) override{
        if(ex){
            //tcp连接失败，直接返回失败
            onWebSocketException(ex);
            return;
        }
        //开始websocket握手
        HttpClientImp::onConnect(ex);
    }

    /**
     * tcp收到数据
     * @param pBuf
     */
    void onRecv(const Buffer::Ptr &pBuf) override{
        if(_onRecv){
            //完成websocket握手后，拦截websocket数据并解析
            _onRecv(pBuf);
        }else{
            //websocket握手数据
            HttpClientImp::onRecv(pBuf);
        }
    }

    /**
     * tcp连接断开
     * @param ex
     */
    void onErr(const SockException &ex) override{
        //tcp断开或者shutdown导致的断开
        onWebSocketException(ex);
    }

    //WebSocketSplitter override

    /**
     * 收到一个webSocket数据包包头，后续将继续触发onWebSocketDecodePlayload回调
     * @param header 数据包头
     */
    void onWebSocketDecodeHeader(const WebSocketHeader &header) override{
        _payload.clear();
    }

    /**
     * 收到webSocket数据包负载
     * @param header 数据包包头
     * @param ptr 负载数据指针
     * @param len 负载数据长度
     * @param recved 已接收数据长度(包含本次数据长度)，等于header._playload_len时则接受完毕
     */
    void onWebSocketDecodePlayload(const WebSocketHeader &header, const uint8_t *ptr, uint64_t len, uint64_t recved) override{
        _payload.append((char *)ptr,len);
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
                shutdown(SockException(Err_eof,"websocket server close the connection"));
            }
                break;
            case WebSocketHeader::PING:{
                //心跳包
                header._opcode = WebSocketHeader::PONG;
                WebSocketSplitter::encode(header,std::make_shared<BufferString>(std::move(_payload)));
            }
                break;
            case WebSocketHeader::CONTINUATION:{

            }
                break;
            case WebSocketHeader::TEXT:
            case WebSocketHeader::BINARY:{
                //接收完毕websocket数据包，触发onRecv事件
                _delegate.onRecv(std::make_shared<BufferString>(std::move(_payload)));
            }
                break;
            default:
                break;
        }
        _payload.clear();
        header._mask_flag = flag;
    }

    /**
     * websocket数据编码回调
     * @param ptr 数据指针
     * @param len 数据指针长度
     */
    void onWebSocketEncodeData(const Buffer::Ptr &buffer) override{
        HttpClientImp::send(buffer);
    }
private:
    void onWebSocketException(const SockException &ex){
        if(!ex){
            //websocket握手成功
            //此处截取TcpClient派生类发送的数据并进行websocket协议打包
            weak_ptr<HttpWsClient> weakSelf = dynamic_pointer_cast<HttpWsClient>(shared_from_this());
            _delegate.setOnBeforeSendCB([weakSelf](const Buffer::Ptr &buf){
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
            _delegate.setSock(HttpClientImp::_sock);
            //触发连接成功事件
            _delegate.onConnect(ex);
            //拦截websocket数据接收
            _onRecv = [this](const Buffer::Ptr &pBuf){
                //解析websocket数据包
                this->WebSocketSplitter::decode((uint8_t*)pBuf->data(),pBuf->size());
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
    string _Sec_WebSocket_Key;
    function<void(const Buffer::Ptr &pBuf)> _onRecv;
    ClientTypeImp<ClientType,DataType> &_delegate;
    string _payload;
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
        _wsClient.reset(new HttpWsClient<ClientType,DataType>(*this));
    }
    ~WebSocketClient() override {}

    /**
     * 重载startConnect方法，
     * 目的是替换TcpClient的连接服务器行为，使之先完成WebSocket握手
     * @param host websocket服务器ip或域名
     * @param iPort websocket服务器端口
     * @param fTimeOutSec 超时时间
     */
    void startConnect(const string &host, uint16_t iPort, float fTimeOutSec = 3) override {
        string ws_url;
        if(useWSS){
            //加密的ws
            ws_url = StrPrinter << "wss://" + host << ":" << iPort << "/" ;
        }else{
            //明文ws
            ws_url = StrPrinter << "ws://" + host << ":" << iPort << "/" ;
        }
        _wsClient->startWsClient(ws_url,fTimeOutSec);
    }

    void startWebSocket(const string &ws_url,float fTimeOutSec = 3){
        _wsClient->startWsClient(ws_url,fTimeOutSec);
    }
private:
    typename HttpWsClient<ClientType,DataType>::Ptr _wsClient;
};

}//namespace mediakit
#endif //ZLMEDIAKIT_WebSocketClient_H
