#ifndef Http_WebSocketClient_h
#define Http_WebSocketClient_h

#include <stdio.h>
#include <string.h>
#include <functional>
#include <memory>
#include "Util/util.h"
#include "Util/mini.h"
#include "Network/TcpClient.h"
#include "Common/Parser.h"
#include "HttpRequestSplitter.h"
#include "HttpCookie.h"
#include "HttpChunkedSplitter.h"
#include "strCoding.h"
#include "Http/HttpClient.h"
#include "Http/WebSocketSplitter.h"
#include "Http/WebSocketSession.h"
#include <cstdlib>
#include <random>
#include "Common/config.h"
#include "Util/SHA1.h"
#include "Util/base64.h"



using namespace std;
using namespace toolkit;

namespace mediakit {

/**
* @brief  客户端的状态
*/
typedef enum WSClientStatus {
    WSCONNECT,
    HANDSHAKING, ///握手中
    WORKING,     ///工作中
} WSClientStatus;

class WebSocketClient : public TcpClient , public HttpRequestSplitter, public WebSocketSplitter
{
public:
    typedef StrCaseMap HttpHeader;
    typedef std::shared_ptr<WebSocketClient> Ptr;
    WebSocketClient() :_WSClientStatus(WSCONNECT) {}
    virtual ~WebSocketClient() {}

    template <typename SessionType>
    void startConnect(const string &strUrl, uint16_t iPort, float fTimeOutSec = 3)
    {
        _ip = strUrl;
        _port = iPort;
        TcpClient::startConnect(strUrl, iPort, fTimeOutSec);

        typedef function<int(const Buffer::Ptr &buf)> onBeforeSendCB;
        /**
         * 该类实现了TcpSession派生类发送数据的截取
         * 目的是发送业务数据前进行websocket协议的打包
         */
        class SessionImp : public SessionType {
        public:
            SessionImp(const Socket::Ptr &pSock) :SessionType(pSock) {}

            ~SessionImp() {}

            /**
             * 设置发送数据截取回调函数
             * @param cb 截取回调函数
             */
            void setOnBeforeSendCB(const onBeforeSendCB &cb) {
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
                return SessionType::send(buf);
            }
        private:
            onBeforeSendCB _beforeSendCB;
        };
        std::shared_ptr<SessionImp> temSession = std::make_shared<SessionImp>(_sock);
        //此处截取数据并进行websocket协议打包
        weak_ptr<WebSocketClient> weakSelf = dynamic_pointer_cast<WebSocketClient>(WebSocketClient::shared_from_this());

        _sock->setOnErr([weakSelf](const SockException &ex) {
            auto strongSelf = weakSelf.lock();
            if (!strongSelf) {
                return;
            }
            strongSelf->onErr(ex);
        });

        temSession->setOnBeforeSendCB([weakSelf](const Buffer::Ptr &buf) {
            auto strongSelf = weakSelf.lock();
            if (strongSelf) {
                WebSocketHeader header;
                header._fin = true;
                header._reserved = 0;
                header._opcode = WebSocketHeader::BINARY;
                header._mask_flag = false;
                strongSelf->WebSocketSplitter::encode(header, (uint8_t *)buf->data(), buf->size());
            }
            return buf->size();
        });
        _session = temSession;
        _session->onManager();
    }

    virtual int send(const string& buf);

    virtual void clear();

    const string &responseStatus() const;

    const HttpHeader &responseHeader() const;

    const Parser& response() const;

    const string &getUrl() const;

protected:
    virtual int64_t onResponseHeader(const string &status,const HttpHeader &headers);;

    virtual void onResponseBody(const char *buf,int64_t size,int64_t recvedSize,int64_t totalSize);;

    /**
     * 接收http回复完毕,
     */
    virtual void onResponseCompleted();

    /**
     * http链接断开回调
     * @param ex 断开原因
     */
    virtual void onDisconnect(const SockException &ex){}

    //HttpRequestSplitter override
    int64_t onRecvHeader(const char *data, uint64_t len) override;

    void onRecvContent(const char *data, uint64_t len) override;

protected:
    virtual void onConnect(const SockException &ex) override;

    virtual void onRecv(const Buffer::Ptr &pBuf) override;

    virtual void onErr(const SockException &ex) override;

    virtual void onFlush() override {};

    virtual void onManager() override;

protected:
    string generate_websocket_client_handshake(const char* ip, uint16_t port, const char * path, const char * key);

    string get_random(size_t n);

    void onWebSocketDecodeHeader(const WebSocketHeader &packet) override;

    void onWebSocketDecodePlayload(const WebSocketHeader &packet, const uint8_t *ptr, uint64_t len, uint64_t recved) override;

    void onWebSocketDecodeComplete(const WebSocketHeader &header_in) override;

    virtual void onWebSocketEncodeData(const uint8_t *ptr, uint64_t len);

private:
    void onResponseCompleted_l();

protected:
    bool _isHttps;
private:
    string _ip;
    int _port;
    string _url;
    string _method;
    string _path;
    //recv
    int64_t _recvedBodySize;
    int64_t _totalBodySize;
    Parser _parser;
    string _lastHost;
    Ticker _aliveTicker;
    float _fTimeOutSec = 0;
    std::shared_ptr<HttpChunkedSplitter> _chunkedSplitter;

    std::string _key;    ///客户端的key
    WSClientStatus _WSClientStatus;  ///客户端状态
    bool _firstPacket = true;
    string _remian_data;

    std::shared_ptr<TcpSession> _session;

};

} /* namespace mediakit */

#endif /* Http_HttpClient_h */
