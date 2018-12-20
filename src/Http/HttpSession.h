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
#include "RtmpMuxer/FlvMuxer.h"
#include "HttpRequestSplitter.h"
#include "WebSocketSplitter.h"

using namespace std;
using namespace toolkit;

namespace mediakit {

class HttpSession: public TcpSession,
                   public FlvMuxer,
                   public HttpRequestSplitter,
                   public WebSocketSplitter {
public:
	typedef StrCaseMap KeyValue;
	typedef std::function<void(const string &codeOut,
							   const KeyValue &headerOut,
							   const string &contentOut)>  HttpResponseInvoker;

	HttpSession(const Socket::Ptr &pSock);
	virtual ~HttpSession();

	virtual void onRecv(const Buffer::Ptr &) override;
	virtual void onError(const SockException &err) override;
	virtual void onManager() override;

	static string urlDecode(const string &str);
protected:
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
	Parser _parser;
	string _strPath;
	Ticker _ticker;
	uint32_t _iReqCnt = 0;
	//消耗的总流量
	uint64_t _ui64TotalBytes = 0;
	//flv over http
    MediaInfo _mediaInfo;
    //处理content数据的callback
	function<bool (const char *data,uint64_t len) > _contentCallBack;
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


typedef TcpSessionWithSSL<HttpSession> HttpsSession;

} /* namespace mediakit */

#endif /* SRC_HTTP_HTTPSESSION_H_ */
