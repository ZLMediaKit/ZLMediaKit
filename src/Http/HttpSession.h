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
#ifndef SRC_HTTP_HTTPSESSION_H_
#define SRC_HTTP_HTTPSESSION_H_

#include <functional>
#include "Common/config.h"
#include "Common/Parser.h"
#include "Network/TcpSession.h"
#include "Network/TcpServer.h"
#include "Rtmp/RtmpMediaSource.h"
#include "Rtmp/FlvMuxer.h"
#include "HttpRequestSplitter.h"
#include "WebSocketSplitter.h"
#include "HttpCookieManager.h"

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

	/**
	 * @param errMsg 如果为空，则代表鉴权通过，否则为错误提示
	 * @param accessPath 运行或禁止访问的根目录
	 * @param cookieLifeSecond 鉴权cookie有效期
	 **/
	typedef std::function<void(const string &errMsg,const string &accessPath, int cookieLifeSecond)> HttpAccessPathInvoker;

	HttpSession(const Socket::Ptr &pSock);
	virtual ~HttpSession();

	virtual void onRecv(const Buffer::Ptr &) override;
	virtual void onError(const SockException &err) override;
	virtual void onManager() override;

	static string urlDecode(const string &str);
protected:
	//FlvMuxer override
	void onWrite(const Buffer::Ptr &data) override ;
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
        shutdown(SockException(Err_shutdown,"http post content is too huge,default closed"));
	}

    void onWebSocketDecodeHeader(const WebSocketHeader &packet) override{
        shutdown(SockException(Err_shutdown,"websocket connection default closed"));
    };

	void onRecvWebSocketData(const Parser &header,const char *data,uint64_t len){
        WebSocketSplitter::decode((uint8_t *)data,len);
    }

private:
    //chenxiaolei  增加OPTIONS,以便支持web 页面的跨域嗅探请求
	inline void Handle_Req_OPTIONS(int64_t &content_len);
	inline void Handle_Req_GET(int64_t &content_len);
	inline void Handle_Req_POST(int64_t &content_len);
	inline bool checkLiveFlvStream();
	inline bool checkWebSocket();
	inline bool emitHttpEvent(bool doInvoke);
	inline void urlDecode(Parser &parser);
	inline void sendNotFound(bool bClose);
	inline void sendResponse(const char *pcStatus,const KeyValue &header,const string &strContent);
	inline static KeyValue makeHttpHeader(bool bClose=false,int64_t iContentSize=-1,const char *pcContentType="text/html");
    void responseDelay(const string &Origin,
                       bool bClose,
                       const string &codeOut,
                       const KeyValue &headerOut,
                       const string &contentOut);

    /**
     * 判断http客户端是否有权限访问文件的逻辑步骤
     *
     * 1、根据http请求头查找cookie，找到进入步骤3
     * 2、根据http url参数(如果没有根据ip+端口号)查找cookie，如果还是未找到cookie则进入步骤5
     * 3、cookie标记是否有权限访问文件，如果有权限，直接返回文件
     * 4、cookie中记录的url参数是否跟本次url参数一致，如果一致直接返回客户端错误码
     * 5、触发kBroadcastHttpAccess事件
     * @param path 文件或目录
     * @param is_dir path是否为目录
     * @param callback 有权限或无权限的回调
     */
    inline void canAccessPath(const string &path,bool is_dir,const function<void(const string &errMsg,const HttpServerCookie::Ptr &cookie)> &callback);

    /**
     * 获取用户唯一识别id
     * 有url参数返回参数，无参数返回ip+端口号
     * @return
     */
    inline string getClientUid();
private:
    Parser _parser;
    Ticker _ticker;
    uint32_t _iReqCnt = 0;
    //消耗的总流量
    uint64_t _ui64TotalBytes = 0;
    //flv over http
    MediaInfo _mediaInfo;
    //处理content数据的callback
    function<bool (const char *data,uint64_t len) > _contentCallBack;
};


typedef TcpSessionWithSSL<HttpSession> HttpsSession;

} /* namespace mediakit */

#endif /* SRC_HTTP_HTTPSESSION_H_ */
