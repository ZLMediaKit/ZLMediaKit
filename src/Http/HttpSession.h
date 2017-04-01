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

using namespace std;
using namespace ZL::Network;

namespace ZL {
namespace Http {

class HttpSession: public TcpLimitedSession<MAX_TCP_SESSION> {
public:
	typedef map<string,string> KeyValue;
	HttpSession(const std::shared_ptr<ThreadPool> &pTh, const Socket::Ptr &pSock);
	virtual ~HttpSession();

	void onRecv(const Socket::Buffer::Ptr &) override;
	void onError(const SockException &err) override;
	void onManager() override;
private:
	typedef int (HttpSession::*HttpCMDHandle)();
	static unordered_map<string, HttpCMDHandle> g_mapCmdIndex;
	Parser m_parser;
	string m_strPath;
	string m_strRcvBuf;
	Ticker m_ticker;
	uint32_t m_iReqCnt = 0;

	inline int parserHttpReq(const string &);
	inline int Handle_Req_GET();
	inline int Handle_Req_POST();
	inline bool makeMeun(const string &strFullPath, string &strRet);
	inline void sendNotFound(bool bClose);
	inline void sendResponse(const char *pcStatus,const KeyValue &header,const string &strContent);
	inline KeyValue makeHttpHeader(bool bClose=false,int64_t iContentSize=-1,const char *pcContentType="text/html");
};

} /* namespace Http */
} /* namespace ZL */

#endif /* SRC_HTTP_HTTPSESSION_H_ */
