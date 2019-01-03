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

#include "Http/HttpClientImp.h"

namespace mediakit {

#if defined(ENABLE_OPENSSL)
void HttpClientImp::onBeforeConnect(string &strUrl, uint16_t &iPort,float &fTimeOutSec) {
	if(_isHttps){
		_sslBox.reset(new SSL_Box(false));
		_sslBox->setOnDecData([this](const char *data, uint32_t len){
			public_onRecvBytes(data,len);
		});
		_sslBox->setOnEncData([this](const char *data, uint32_t len){
			public_send(data,len);
		});
	}else{
		_sslBox.reset();
	}
}

void HttpClientImp::onRecvBytes(const char* data, int size) {
	if(_sslBox){
		_sslBox->onRecv(data,size);
	}else{
		HttpClient::onRecvBytes(data,size);
	}
}

int HttpClientImp::send(const Buffer::Ptr &buf) {
	if(_sslBox){
		_sslBox->onSend(buf->data(),buf->size());
		return buf->size();
	}
	return HttpClient::send(buf);
}

#endif //defined(ENABLE_OPENSSL)

} /* namespace mediakit */
