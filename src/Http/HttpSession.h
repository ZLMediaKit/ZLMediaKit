/*
 * HttpSession.h
 *
 *  Created on: 2016年9月22日
 *      Author: xzl
 */

#ifndef SRC_HTTP_HTTPSESSION_H_
#define SRC_HTTP_HTTPSESSION_H_

#include "config.h"
#include "Rtsp/Rtsp.h"
#include "Network/TcpLimitedSession.h"
#include <functional>

using namespace std;
using namespace ZL::Network;
using namespace ZL::Network;

namespace ZL {
namespace Http {


class HttpSession: public TcpLimitedSession<MAX_TCP_SESSION> {
public:
	typedef map<string,string> KeyValue;
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
