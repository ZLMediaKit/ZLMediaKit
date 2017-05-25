/*
 * HttpClientImp.h
 *
 *  Created on: 2017年5月4日
 *      Author: xzl
 */

#ifndef SRC_HTTP_HTTPCLIENTIMP_H_
#define SRC_HTTP_HTTPCLIENTIMP_H_

#include "HttpClient.h"
#ifdef ENABLE_OPENSSL
#include "Util/SSLBox.h"
using namespace ZL::Util;
#endif //ENABLE_OPENSSL

namespace ZL {
namespace Http {

class HttpClientImp: public HttpClient {
public:
	typedef std::shared_ptr<HttpClientImp> Ptr;
	HttpClientImp();
	virtual ~HttpClientImp();
	virtual void sendRequest(const string &url) override;

#ifdef ANDROID
	void public_onRecvBytes(const char *data,int len){
		HttpClient::onRecvBytes(data,len);
	}
	void public_send(const char *data, uint32_t len){
		HttpClient::send(data,len);
	}
#endif //ANDROID
private:
#ifdef ENABLE_OPENSSL
	virtual void onRecvBytes(const char *data,int size) override;
	virtual int send(const string &str) override;
	virtual int send(const char *str, int len) override;
	std::shared_ptr<SSL_Box> _sslBox;
#endif //ENABLE_OPENSSL
};

} /* namespace Http */
} /* namespace ZL */

#endif /* SRC_HTTP_HTTPCLIENTIMP_H_ */
