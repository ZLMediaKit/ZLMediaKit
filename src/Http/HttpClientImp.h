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
	virtual void sendRequest(const string &url,float fTimeOutSec) override;

#if defined(__GNUC__) && (__GNUC__ < 5)
	void public_onRecvBytes(const char *data,int len){
		HttpClient::onRecvBytes(data,len);
	}
	void public_send(const char *data, uint32_t len){
		HttpClient::send(data,len);
	}
#endif //defined(__GNUC__) && (__GNUC__ < 5)
private:
#ifdef ENABLE_OPENSSL
	virtual void onRecvBytes(const char *data,int size) override;
	virtual int send(const string &str) override;
	virtual int send(string &&str) override;
	virtual int send(const char *str, int len) override;
	std::shared_ptr<SSL_Box> _sslBox;
#endif //ENABLE_OPENSSL
};

} /* namespace Http */
} /* namespace ZL */

#endif /* SRC_HTTP_HTTPCLIENTIMP_H_ */
