/*
 * HttpClientImp.cpp
 *
 *  Created on: 2017年5月4日
 *      Author: xzl
 */

#include <Http/HttpClientImp.h>

namespace ZL {
namespace Http {

HttpClientImp::HttpClientImp() {
	// TODO Auto-generated constructor stub

}

HttpClientImp::~HttpClientImp() {
}

void HttpClientImp::sendRequest(const string& url) {
	HttpClient::sendRequest(url);
	if(_isHttps){
#ifndef ENABLE_OPENSSL
		shutdown();
		throw std::invalid_argument("不支持HTTPS协议");
#else
		_sslBox.reset(new SSL_Box(false));
		_sslBox->setOnDecData([this](const char *data, uint32_t len){
			HttpClient::onRecvBytes(data,len);
		});
		_sslBox->setOnEncData([this](const char *data, uint32_t len){
			HttpClient::send(data,len);
		});
#endif //ENABLE_OPENSSL

	}else{
#ifdef ENABLE_OPENSSL
		_sslBox.reset();
#endif //ENABLE_OPENSSL
	}
}

#ifdef ENABLE_OPENSSL
void HttpClientImp::onRecvBytes(const char* data, int size) {
	if(_sslBox){
		_sslBox->onRecv(data,size);
	}else{
		HttpClient::onRecvBytes(data,size);
	}
}

int HttpClientImp::send(const string& str) {
	if(_sslBox){
		_sslBox->onSend(str.data(),str.size());
		return str.size();
	}
	return HttpClient::send(str);
}

int HttpClientImp::send(const char* str, int len) {
	if(_sslBox){
		_sslBox->onSend(str,len);
		return len;
	}
	return HttpClient::send(str,len);

}
#endif //ENABLE_OPENSSL

} /* namespace Http */
} /* namespace ZL */
