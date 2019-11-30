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

#ifndef ZLMEDIAKIT_WEBSOCKETSESSION_H
#define ZLMEDIAKIT_WEBSOCKETSESSION_H

#include "HttpSession.h"
#include "Network/TcpServer.h"

/**
* 通过该模板类可以透明化WebSocket协议，
* 用户只要实现WebSock协议下的具体业务协议，譬如基于WebSocket协议的Rtmp协议等
* @tparam SessionType 业务协议的TcpSession类
*/
template <class SessionType,class HttpSessionType = HttpSession,WebSocketHeader::Type DataType = WebSocketHeader::TEXT>
class WebSocketSession : public HttpSessionType {
public:
    WebSocketSession(const Socket::Ptr &pSock) : HttpSessionType(pSock){}
    virtual ~WebSocketSession(){}

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
        _session = std::make_shared<SessionImp>(HttpSessionType::getIdentifier(),HttpSessionType::_sock);
        auto strongServer = _weakServer.lock();
        if(strongServer){
            _session->attachServer(*strongServer);
        }

        //此处截取数据并进行websocket协议打包
        weak_ptr<WebSocketSession> weakSelf = dynamic_pointer_cast<WebSocketSession>(HttpSessionType::shared_from_this());
        _session->setOnBeforeSendCB([weakSelf](const Buffer::Ptr &buf){
            auto strongSelf = weakSelf.lock();
            if(strongSelf){
                WebSocketHeader header;
                header._fin = true;
                header._reserved = 0;
                header._opcode = DataType;
                header._mask_flag = false;
                strongSelf->WebSocketSplitter::encode(header,buf);
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
    void onWebSocketDecodePlayload(const WebSocketHeader &packet,const uint8_t *ptr,uint64_t len,uint64_t recved) override {
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
        SocketHelper::send(buffer);
    }
private:
    typedef function<int(const Buffer::Ptr &buf)> onBeforeSendCB;
    /**
     * 该类实现了TcpSession派生类发送数据的截取
     * 目的是发送业务数据前进行websocket协议的打包
     */
    class SessionImp : public SessionType{
    public:
        SessionImp(const string &identifier,const Socket::Ptr &pSock) :
                _identifier(identifier),SessionType(pSock){}

        ~SessionImp(){}

        /**
         * 设置发送数据截取回调函数
         * @param cb 截取回调函数
         */
        void setOnBeforeSendCB(const onBeforeSendCB &cb){
            _beforeSendCB = cb;
        }
    protected:
        /**
         * 重载send函数截取数据
         * @param buf 需要截取的数据
         * @return 数据字节数
         */
        int send(const Buffer::Ptr &buf) override {
            if(_beforeSendCB){
                return _beforeSendCB(buf);
            }
            return SessionType::send(buf);
        }
        string getIdentifier() const override{
            return _identifier;
        }
    private:
        onBeforeSendCB _beforeSendCB;
        string _identifier;
    };
private:
    string _remian_data;
    weak_ptr<TcpServer> _weakServer;
    std::shared_ptr<SessionImp> _session;
};


#endif //ZLMEDIAKIT_WEBSOCKETSESSION_H
