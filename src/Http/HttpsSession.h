/*
 * HttpsSession.h
 *
 *  Created on: 2017年4月19日
 *      Author: xzl
 */

#ifndef SRC_HTTP_HTTPSSESSION_H_
#define SRC_HTTP_HTTPSSESSION_H_

#include "HttpSession.h"
#include "Util/SSLBox.h"
#include "Util/TimeTicker.h"

using namespace ZL::Util;

namespace ZL {
namespace Http {

class HttpsSession: public HttpSession {
public:
	HttpsSession(const std::shared_ptr<ThreadPool> &pTh, const Socket::Ptr &pSock):
		HttpSession(pTh,pSock){
		m_sslBox.setOnEncData([&](const char *data, uint32_t len){
#if defined(__GNUC__) && (__GNUC__ < 5)
			public_send(data,len);
#else//defined(__GNUC__) && (__GNUC__ < 5)
			HttpSession::send(data,len);
#endif//defined(__GNUC__) && (__GNUC__ < 5)
		});
		m_sslBox.setOnDecData([&](const char *data, uint32_t len){
#if defined(__GNUC__) && (__GNUC__ < 5)
			public_onRecv(data,len);
#else//defined(__GNUC__) && (__GNUC__ < 5)
			HttpSession::onRecv(data,len);
#endif//defined(__GNUC__) && (__GNUC__ < 5)
		});
	}
	virtual ~HttpsSession(){
		//m_sslBox.shutdown();
	}
	void onRecv(const Socket::Buffer::Ptr &pBuf) override{
		TimeTicker();
		m_sslBox.onRecv(pBuf->data(), pBuf->size());
	}
#if defined(__GNUC__) && (__GNUC__ < 5)
	int public_send(const char *data, uint32_t len){
		return HttpSession::send(data,len);
	}
	void public_onRecv(const char *data, uint32_t len){
		HttpSession::onRecv(data,len);
	}
#endif//defined(__GNUC__) && (__GNUC__ < 5)
private:
	virtual int send(const string &buf) override{
		TimeTicker();
		m_sslBox.onSend(buf.data(), buf.size());
		return buf.size();
	}
	virtual int send(const char *buf, int size) override{
		TimeTicker();
		m_sslBox.onSend(buf, size);
		return size;
	}
	SSL_Box m_sslBox;
};

} /* namespace Http */
} /* namespace ZL */

#endif /* SRC_HTTP_HTTPSSESSION_H_ */
