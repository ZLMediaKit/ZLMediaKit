/*
 * MIT License
 *
 * Copyright (c) 2016 xiongziliang <771730766@qq.com>
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
#ifndef SRC_HTTP_HTTPSESSION_H_
#define SRC_HTTP_HTTPSESSION_H_

#include <functional>
#include "Common/config.h"
#include "Rtsp/Rtsp.h"
#include "Network/TcpSession.h"
#include "Network/TcpServer.h"
#include "Rtmp/RtmpMediaSource.h"
#include "Rtmp/FlvMuxer.h"
#include "HttpRequestSplitter.h"
#include "WebSocketSplitter.h"

using namespace std;
using namespace ZL::Rtmp;
using namespace ZL::Network;

namespace ZL {
namespace Http {

class HttpSession: public TcpSession,
                   public FlvMuxer,
                   public HttpRequestSplitter,
                   public WebSocketSplitter {
public:
	typedef StrCaseMap KeyValue;
	typedef std::function<void(const string &codeOut,
							   const KeyValue &headerOut,
							   const string &contentOut)>  HttpResponseInvoker;

	HttpSession(const std::shared_ptr<ThreadPool> &pTh, const Socket::Ptr &pSock);
	virtual ~HttpSession();

	virtual void onRecv(const Buffer::Ptr &) override;
	virtual void onError(const SockException &err) override;
	virtual void onManager() override;

	static string urlDecode(const string &str);
protected:
	//用于HttpsSession调用
	void onRecv(const char *data,int size);
	//FlvMuxer override
	void onWrite(const Buffer::Ptr &data) override ;
	void onWrite(const char *data,int len) override;
	void onDetach() override;
	std::shared_ptr<FlvMuxer> getSharedPtr() override;
	//HttpRequestSplitter override

	int64_t onRecvHeader(const char *data,uint64_t len) override;
	void onRecvContent(const char *data,uint64_t len) override;

	/**
	 * 重载之用于处理不定长度的content
	 * 这个函数可用于处理大文件上传、http-flv推流
	 * @param header http请求头
	 * @param data content分片数据
	 * @param len content分片数据大小
	 * @param totalSize content总大小,如果为0则是不限长度content
	 * @param recvedSize 已收数据大小
	 */
	virtual void onRecvUnlimitedContent(const Parser &header,
										const char *data,
										uint64_t len,
										uint64_t totalSize,
										uint64_t recvedSize){
        WarnL << "content数据长度过大，无法处理,请重载HttpSession::onRecvUnlimitedContent";
        shutdown();
	}

    void onWebSocketDecodeHeader(const WebSocketHeader &packet) override{
        DebugL << "默认关闭WebSocket";
        shutdown();
    };

	void onRecvWebSocketData(const Parser &header,const char *data,uint64_t len){
        WebSocketSplitter::decode((uint8_t *)data,len);
    }
private:
	Parser m_parser;
	string m_strPath;
	Ticker m_ticker;
	uint32_t m_iReqCnt = 0;
	//消耗的总流量
	uint64_t m_ui64TotalBytes = 0;
	//flv over http
    MediaInfo m_mediaInfo;
    //处理content数据的callback
	function<bool (const char *data,uint64_t len) > m_contentCallBack;
private:
	inline bool Handle_Req_GET(int64_t &content_len);
	inline bool Handle_Req_POST(int64_t &content_len);
	inline bool checkLiveFlvStream();
	inline bool checkWebSocket();
	inline bool emitHttpEvent(bool doInvoke);
	inline void urlDecode(Parser &parser);
	inline bool makeMeun(const string &strFullPath,const string &vhost, string &strRet);
	inline void sendNotFound(bool bClose);
	inline void sendResponse(const char *pcStatus,const KeyValue &header,const string &strContent);
	inline static KeyValue makeHttpHeader(bool bClose=false,int64_t iContentSize=-1,const char *pcContentType="text/html");
	void responseDelay(const string &Origin,bool bClose,
					   const string &codeOut,const KeyValue &headerOut,
					   const string &contentOut);
};


/**
 * 通过该模板类可以透明化WebSocket协议，
 * 用户只要实现WebSock协议下的具体业务协议，譬如基于WebSocket协议的Rtmp协议等
 * @tparam SessionType 业务协议的TcpSession类
 */
template <typename SessionType>
class WebSocketSession : public HttpSession {
public:
    WebSocketSession(const std::shared_ptr<ThreadPool> &pTh, const Socket::Ptr &pSock) : HttpSession(pTh,pSock){}
    virtual ~WebSocketSession(){}

    //收到eof或其他导致脱离TcpServer事件的回调
    void onError(const SockException &err) override{
        HttpSession::onError(err);
        if(_session){
            _session->onError(err);
        }
    }
    //每隔一段时间触发，用来做超时管理
    void onManager() override{
        HttpSession::onManager();
        if(_session){
            _session->onManager();
        }
    }

    void attachServer(const TcpServer &server) override{
        HttpSession::attachServer(server);
        _weakServer = const_cast<TcpServer &>(server).shared_from_this();
    }
protected:
    /**
     * 开始收到一个webSocket数据包
     * @param packet
     */
    void onWebSocketDecodeHeader(const WebSocketHeader &packet) override{
        //新包，原来的包残余数据清空掉
        _remian_data.clear();

        if(!_firstPacket){
            return;
        }
        //这是个WebSocket会话而不是普通的Http会话
        _firstPacket = false;
        _session = std::make_shared<SessionImp>(getIdentifier(),nullptr,_sock);

        auto strongServer = _weakServer.lock();
        if(strongServer){
            _session->attachServer(*strongServer);
        }

        //此处截取数据并进行websocket协议打包
        weak_ptr<WebSocketSession> weakSelf = dynamic_pointer_cast<WebSocketSession>(shared_from_this());
        _session->setOnBeforeSendCB([weakSelf](const Buffer::Ptr &buf){
            auto strongSelf = weakSelf.lock();
            if(strongSelf){
                bool mask_flag = strongSelf->_mask_flag;
                strongSelf->_mask_flag = false;
                strongSelf->WebSocketSplitter::encode((uint8_t *)buf->data(),buf->size());
                strongSelf->_mask_flag = mask_flag;
            }
            return buf->size();
        });

    }

    /**
     * 收到websocket数据包负载
     * @param packet
     * @param ptr
     * @param len
     * @param recved
     */
    void onWebSocketDecodePlayload(const WebSocketHeader &packet,const uint8_t *ptr,uint64_t len,uint64_t recved) override {
        if(packet._playload_len == recved){
            //收到完整的包
            if(_remian_data.empty()){
                onRecvWholePacket((char *)ptr,len);
            }else{
                _remian_data.append((char *)ptr,len);
                onRecvWholePacket(_remian_data);
                _remian_data.clear();
            }
        } else {
            //部分数据
            _remian_data.append((char *)ptr,len);
        }
    }

    /**
     * 发送数据进行websocket协议打包后回调
     * @param ptr
     * @param len
     */
    void onWebSocketEncodeData(const uint8_t *ptr,uint64_t len) override{
        _session->realSend(_session->obtainBuffer((char *)ptr,len));
    }

    /**
     * 收到一个完整的websock数据包
     * @param data
     * @param len
     */
    void onRecvWholePacket(const char *data,uint64_t len){
        BufferRaw::Ptr buffer = _session->obtainBuffer(data,len);
        _session->onRecv(buffer);
    }

    /**
     * 收到一个完整的websock数据包
     * @param str
     */
    void onRecvWholePacket(const string &str){
        BufferString::Ptr buffer = std::make_shared<BufferString>(str);
        _session->onRecv(buffer);
    }

private:
    typedef function<int(const Buffer::Ptr &buf)> onBeforeSendCB;
    /**
     * 该类实现了TcpSession派生类发送数据的截取
     * 目的是发送业务数据前进行websocket协议的打包
     */
    class SessionImp : public SessionType{
    public:
        SessionImp(const string &identifier,
                   const std::shared_ptr<ThreadPool> &pTh,
                   const Socket::Ptr &pSock) :
                _identifier(identifier),SessionType(pTh,pSock){}

        ~SessionImp(){}

        /**
         * 截取到数据后，再进行webSocket协议打包
         * 然后真正的发送数据到socket
         * @param buf 数据
         * @return 数据字节数
         */
        int realSend(const Buffer::Ptr &buf){
            return SessionType::send(buf);
        }

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
    bool _firstPacket = true;
    string _remian_data;
    weak_ptr<TcpServer> _weakServer;
    std::shared_ptr<SessionImp> _session;
};

/**
 * 回显会话
 */
class EchoSession : public TcpSession {
public:
    EchoSession(const std::shared_ptr<ThreadPool> &pTh, const Socket::Ptr &pSock) : TcpSession(pTh,pSock){};
    virtual ~EchoSession(){};

    void onRecv(const Buffer::Ptr &buffer) override {
        send(buffer);
    }
    void onError(const SockException &err) override{
        WarnL << err.what();
    }
    //每隔一段时间触发，用来做超时管理
    void onManager() override{
        DebugL;
    }
};


typedef WebSocketSession<EchoSession> EchoWebSocketSession;

} /* namespace Http */
} /* namespace ZL */

#endif /* SRC_HTTP_HTTPSESSION_H_ */
