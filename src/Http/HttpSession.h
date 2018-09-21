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

	/**
     * 收到请求头
     * @param header 请求头
     * @return 请求头后的content长度,
     *  <0 : 代表后面所有数据都是content
     *  0 : 代表为后面数据还是请求头,
     *  >0 : 代表后面数据为固定长度content,
     */
	int64_t onRecvHeader(const string &header) override;

	/**
     * 收到content分片或全部数据
     * onRecvHeader函数返回>0,则为全部数据
     * @param content
     */
	void onRecvContent(const string &content) override;

	/**
	 * 重载之用于处理不定长度的content
	 * 这个函数可用于处理大文件上传、http-flv推流
	 * @param header http请求头
	 * @param content content分片数据
	 * @param totalSize content总大小,如果为0则是不限长度content
	 * @param recvedSize 已收数据大小
	 */
	virtual void onRecvUnlimitedContent(const Parser &header,const string &content,int64_t totalSize,int64_t recvedSize){
        WarnL << "content数据长度过大，无法处理,请重载HttpSession::onRecvUnlimitedContent";
        shutdown();
	}


    /**
     * 重载之用于处理websocket数据
     * @param header http请求头
     * @param data websocket数据
     */
	virtual void onRecvWebSocketData(const Parser &header,const string &data){
        WebSocketSplitter::decode((uint8_t *)data.data(),data.size());
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
	function<bool (const string &content) > m_contentCallBack;
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
 * 回显WebSocket会话
 */
class EchoWebSocketSession : public HttpSession {
public:
    EchoWebSocketSession(const std::shared_ptr<ThreadPool> &pTh, const Socket::Ptr &pSock) : HttpSession(pTh,pSock){};
    virtual ~EchoWebSocketSession(){};
protected:
    void onWebSocketDecodeHeader(const WebSocketHeader &packet) override{
        DebugL << packet._playload_len;
    };
    void onWebSocketDecodePlayload(const WebSocketHeader &packet,const uint8_t *ptr,uint64_t len,uint64_t recved) override {
        DebugL << string((char *)ptr,len) << " " << recved;

        //webSocket服务器不允许对数据进行掩码加密
        bool mask_flag = _mask_flag;
        _mask_flag = false;
        WebSocketSplitter::encode((uint8_t *)ptr,len);
        _mask_flag = true;

    };

    void onWebSocketEncodeData(const uint8_t *ptr,uint64_t len) override{
        send((char *)ptr,len);
    };

};

} /* namespace Http */
} /* namespace ZL */

#endif /* SRC_HTTP_HTTPSESSION_H_ */
