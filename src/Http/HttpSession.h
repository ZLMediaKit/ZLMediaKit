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

using namespace std;
using namespace ZL::Rtmp;
using namespace ZL::Network;

namespace ZL {
namespace Http {


class HttpSession: public TcpSession,public FlvMuxer {
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
	void onRecv(const char *data,int size);

	//FlvMuxer override
	void onWrite(const Buffer::Ptr &data) override ;
	void onWrite(const char *data,int len) override;
	void onDetach() override;
	std::shared_ptr<FlvMuxer> getSharedPtr() override;
private:
	typedef enum
	{
		Http_success = 0,
		Http_failed = 1,
		Http_moreData = 2,
	} HttpCode;

	Parser m_parser;
	string m_strPath;
	string m_strRcvBuf;
	Ticker m_ticker;
	uint32_t m_iReqCnt = 0;
	//消耗的总流量
	uint64_t m_ui64TotalBytes = 0;

	//flv over http
    MediaInfo m_mediaInfo;

	inline HttpCode parserHttpReq(const string &);
	inline HttpCode Handle_Req_GET();
	inline HttpCode Handle_Req_POST();
	inline bool checkLiveFlvStream();
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

} /* namespace Http */
} /* namespace ZL */

#endif /* SRC_HTTP_HTTPSESSION_H_ */
