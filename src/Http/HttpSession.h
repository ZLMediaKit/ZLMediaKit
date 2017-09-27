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
#include "Network/TcpLimitedSession.h"

using namespace std;
using namespace ZL::Network;

namespace ZL {
namespace Http {


class HttpSession: public TcpLimitedSession<MAX_TCP_SESSION> {
public:
	typedef StrCaseMap KeyValue;
	typedef std::function<void(const string &codeOut,
							   const KeyValue &headerOut,
							   const string &contentOut)>  HttpResponseInvoker;

	HttpSession(const std::shared_ptr<ThreadPool> &pTh, const Socket::Ptr &pSock);
	virtual ~HttpSession();

	virtual void onRecv(const Socket::Buffer::Ptr &) override;
	virtual void onError(const SockException &err) override;
	virtual void onManager() override;
protected:
	void onRecv(const char *data,int size);
private:
	typedef enum
	{
		Http_success = 0,
		Http_failed = 1,
		Http_moreData = 2,
	} HttpCode;
	typedef HttpSession::HttpCode (HttpSession::*HttpCMDHandle)();
	static unordered_map<string, HttpCMDHandle> g_mapCmdIndex;

	Parser m_parser;
	string m_strPath;
	string m_strRcvBuf;
	Ticker m_ticker;
	uint32_t m_iReqCnt = 0;


	inline HttpCode parserHttpReq(const string &);
	inline HttpCode Handle_Req_GET();
	inline HttpCode Handle_Req_POST();
	inline bool makeMeun(const string &strFullPath, string &strRet);
	inline void sendNotFound(bool bClose);
	inline void sendResponse(const char *pcStatus,const KeyValue &header,const string &strContent);
	inline static KeyValue makeHttpHeader(bool bClose=false,int64_t iContentSize=-1,const char *pcContentType="text/html");
	void responseDelay(bool bClose,string &codeOut,KeyValue &headerOut, string &contentOut);
};

} /* namespace Http */
} /* namespace ZL */

#endif /* SRC_HTTP_HTTPSESSION_H_ */
