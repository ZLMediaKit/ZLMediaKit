/*
 * Session.h
 *
 *  Created on: 2015年10月27日
 *      Author: root
 */

#ifndef SERVER_SESSION_H_
#define SERVER_SESSION_H_
#include <memory>
#include "Socket.h"
#include "Util/logger.h"
#include "Thread/ThreadPool.h"

using namespace std;
using namespace ZL::Util;
using namespace ZL::Thread;

namespace ZL {
namespace Network {

class TcpSession: public std::enable_shared_from_this<TcpSession> {
public:
	TcpSession(const std::shared_ptr<ThreadPool> &_th, const Socket::Ptr &_sock) :
			sock(_sock), th(_th) {
		localIp = sock->get_local_ip();
		peerIp = sock->get_peer_ip();
		localPort = sock->get_local_port();
		peerPort = sock->get_peer_port();
	}
	virtual ~TcpSession() {
	}
	virtual void onRecv(const Socket::Buffer::Ptr &) =0;
	virtual void onError(const SockException &err) =0;
	virtual void onManager() =0;

	template <typename T>
	void async(T &&task) {
		th->async(std::forward<T>(task));
	}
	template <typename T>
	void async_first(T &&task) {
		th->async_first(std::forward<T>(task));
	}

protected:
	const string& getLocalIp() const {
		return localIp;
	}
	const string& getPeerIp() const {
		return peerIp;
	}
	uint16_t getLocalPort() const {
		return localPort;
	}
	uint16_t getPeerPort() const {
		return peerPort;
	}
	virtual void shutdown() {
		sock->emitErr(SockException(Err_other, "self shutdown"));
	}
	void safeShutdown(){
		std::weak_ptr<TcpSession> weakSelf = shared_from_this();
		async_first([weakSelf](){
			auto strongSelf = weakSelf.lock();
			if(strongSelf){
				strongSelf->shutdown();
			}
		});
	}
	virtual int send(const string &buf) {
		return sock->send(buf);
	}
	virtual int send(const char *buf, int size) {
		return sock->send(buf, size);
	}

	Socket::Ptr sock;
private:
	std::shared_ptr<ThreadPool> th;
	string localIp;
	string peerIp;
	uint16_t localPort;
	uint16_t peerPort;
};


} /* namespace Session */
} /* namespace ZL */

#endif /* SERVER_SESSION_H_ */
