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
	void onRecv(const Buffer::Ptr &pBuf) override{
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
	virtual int send(string &&buf) override{
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
