/*
 * SSLServer.h
 *
 *  Created on: 2016年1月11日
 *      Author: root
 */

#ifndef CRYPTO_SSLBOX_H_
#define CRYPTO_SSLBOX_H_

#include <fcntl.h>
#include <openssl/bio.h>
#include <openssl/ossl_typ.h>
#include <mutex>
#include <string>
#include <atomic>
#include <functional>
#include "logger.h"

#if defined(_WIN32)
#if defined(_WIN64)

//64bit
#if defined(_DEBUG)
#pragma  comment (lib,"libssl64MDd") 
#pragma  comment (lib,"libcrypto64MDd") 
#else
#pragma  comment (lib,"libssl64MD") 
#pragma  comment (lib,"libcrypto64MD") 
#endif // defined(_DEBUG)

#else 

//32 bit
#if defined(_DEBUG)
#pragma  comment (lib,"libssl32MDd") 
#pragma  comment (lib,"libcrypto32MDd") 
#else
#pragma  comment (lib,"libssl32MD") 
#pragma  comment (lib,"libcrypto32MD") 
#endif // defined(_DEBUG)

#endif //defined(_WIN64)
#endif // defined(_WIN32)


using namespace std;

namespace ZL {
namespace Util {

class SSL_Initor {
public:
	friend class SSL_Box;
	static SSL_Initor &Instance() {
		static SSL_Initor obj;
		return obj;
	}
	void loadServerPem(const char *keyAndCA_pem, const char *import_pwd = "");
	void loadClientPem(const char *keyAndCA_pem, const char *import_pwd = "");
private:
	static mutex *_mutexes;
	SSL_CTX *ssl_server;
	SSL_CTX *ssl_client;
	SSL_Initor();
	~SSL_Initor();
	void setCtx(SSL_CTX *ctx);
	void loadPem(SSL_CTX *ctx, const char *keyAndCA_pem,const char *import_pwd);
	inline std::string getLastError();
}
;
class SSL_Box {
public:
	SSL_Box(bool isServer = true, bool enable = true);
	virtual ~SSL_Box();

	//收到密文后，调用此函数解密
	void onRecv(const char *data, uint32_t data_len);
	//需要加密明文调用此函数
	void onSend(const char *data, uint32_t data_len);

	//设置解密后获取明文的回调
	template<typename F>
	void setOnDecData(F &&fun) {
		onDec = fun;
	}

	//设置加密后获取密文的回调
	template<typename F>
	void setOnEncData(F &&fun) {
		onEnc = fun;
	}
	void shutdown();
private:
	bool isServer;
	bool enable;
	bool sendHandshake;
	SSL *ssl;
	BIO *read_bio, *write_bio;
	function<void(const char *data, uint32_t len)> onDec;
	function<void(const char *data, uint32_t len)> onEnc;
	std::string _bufferOut;
	void flush();
	void flushWriteBio(char *buf, int bufsize);
	void flushReadBio(char *buf, int bufsize);

};

} /* namespace Util */
} /* namespace ZL */

#endif /* CRYPTO_SSLBOX_H_ */
