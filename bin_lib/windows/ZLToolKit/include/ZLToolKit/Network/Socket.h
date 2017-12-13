//
//  Socket.h
//  xzl
//
//  Created by xzl on 16/4/13.
//

#ifndef Socket_h
#define Socket_h

#include <memory>
#include <string>
#include <deque>
#include <mutex>
#include <atomic>
#include <functional>
#include "Util/util.h"
#include "Util/TimeTicker.h"
#include "Poller/Timer.h"
#include "Network/sockutil.h"
#include "Thread/spin_mutex.h"

using namespace std;
using namespace ZL::Util;
using namespace ZL::Poller;
using namespace ZL::Thread;

namespace ZL {
namespace Network {

#if defined(MSG_NOSIGNAL)
#define FLAG_NOSIGNAL MSG_NOSIGNAL
#else
#define FLAG_NOSIGNAL 0
#endif //MSG_NOSIGNAL

#if defined(MSG_MORE)
#define FLAG_MORE MSG_MORE
#else
#define FLAG_MORE 0
#endif //MSG_MORE

#if defined(MSG_DONTWAIT)
#define FLAG_DONTWAIT MSG_DONTWAIT
#else
#define FLAG_DONTWAIT 0
#endif //MSG_DONTWAIT

#define TCP_DEFAULE_FLAGS (FLAG_NOSIGNAL | FLAG_DONTWAIT)
#define UDP_DEFAULE_FLAGS (FLAG_NOSIGNAL | FLAG_DONTWAIT)

#define MAX_SEND_PKT (256)

#if defined(__APPLE__)
  #import "TargetConditionals.h"
  #if TARGET_IPHONE_SIMULATOR
    #define OS_IPHONE
  #elif TARGET_OS_IPHONE
    #define OS_IPHONE
  #endif
#endif //__APPLE__

typedef enum {
	Err_success = 0, //成功
	Err_eof, //eof
	Err_timeout, //超时
	Err_refused,
	Err_dns,
	Err_other,
} ErrCode;

class SockException: public std::exception {
public:
	SockException(ErrCode _errCode = Err_success, const string &_errMsg = "") {
		errMsg = _errMsg;
		errCode = _errCode;
	}
	void reset(ErrCode _errCode, const string &_errMsg) {
		errMsg = _errMsg;
		errCode = _errCode;
	}
	virtual const char* what() const noexcept {
		return errMsg.c_str();
	}

	ErrCode getErrCode() const {
		return errCode;
	}
	operator bool() const{
		return errCode != Err_success;
	}
private:
	string errMsg;
	ErrCode errCode;
};
class SockFD
{
public:
	typedef std::shared_ptr<SockFD> Ptr;
	SockFD(int sock){
		_sock = sock;
	}
	virtual ~SockFD(){
        ::shutdown(_sock, SHUT_RDWR);
#if defined (OS_IPHONE)
        unsetSocketOfIOS(_sock);
#endif //OS_IPHONE
        int fd =  _sock;
        EventPoller::Instance().delEvent(fd,[fd](bool){
            close(fd);
        });
	}
	void setConnected(){
#if defined (OS_IPHONE)
		setSocketOfIOS(_sock);
#endif //OS_IPHONE
	}
	int rawFd() const{
		return _sock;
	}
private:
	int _sock;

#if defined (OS_IPHONE)
	void *readStream=NULL;
	void *writeStream=NULL;
	bool setSocketOfIOS(int socket);
	void unsetSocketOfIOS(int socket);
#endif //OS_IPHONE
};

class Socket: public std::enable_shared_from_this<Socket> {
public:
	class Buffer {
	public:
		typedef std::shared_ptr<Buffer> Ptr;
		Buffer(uint32_t size) {
			_size = size;
			_data = new char[size];
		}
		virtual ~Buffer() {
			delete[] _data;
		}
		const char *data() const {
			return _data;
		}
		uint32_t size() const {
			return _size;
		}
	private:
		friend class Socket;
		char *_data;
		uint32_t _size;
	};
	typedef std::shared_ptr<Socket> Ptr;
	typedef function<void(const Buffer::Ptr &buf, struct sockaddr *addr)> onReadCB;
	typedef function<void(const SockException &err)> onErrCB;
	typedef function<void(Socket::Ptr &sock)> onAcceptCB;
	typedef function<bool()> onFlush;

	Socket();
	virtual ~Socket();
	int rawFD() const{
		SockFD::Ptr sock;
		{
			lock_guard<spin_mutex> lck(_mtx_sockFd);
			sock = _sockFd;
		}
		if(!sock){
			return -1;
		}
		return sock->rawFd();
	}
	void connect(const string &url, uint16_t port, onErrCB &&connectCB, int timeoutSec = 5);
	bool listen(const uint16_t port, const char *localIp = "0.0.0.0", int backLog = 1024);
	bool bindUdpSock(const uint16_t port, const char *localIp = "0.0.0.0");

	void setOnRead(const onReadCB &cb);
	void setOnErr(const onErrCB &cb);
	void setOnAccept(const onAcceptCB &cb);
	void setOnFlush(const onFlush &cb);

	int send(const char *buf, int size = 0,int flags = TCP_DEFAULE_FLAGS);
	int send(const string &buf,int flags = TCP_DEFAULE_FLAGS);
	int sendTo(const char *buf, int size, struct sockaddr *peerAddr,int flags = UDP_DEFAULE_FLAGS);
	int sendTo(const string &buf, struct sockaddr *peerAddr,int flags = UDP_DEFAULE_FLAGS);
	bool emitErr(const SockException &err);
	void enableRecv(bool enabled);

	string get_local_ip();
	uint16_t get_local_port();
	string get_peer_ip();
	uint16_t get_peer_port();

	void setSendPktSize(uint32_t iPktSize){
		_iMaxSendPktSize = iPktSize;
	}
private:
 	mutable spin_mutex _mtx_sockFd;
	SockFD::Ptr _sockFd;
	//send buffer
	recursive_mutex _mtx_sendBuf;
	deque<string> _sendPktBuf;
	deque<struct sockaddr> _udpSendPeer;
	/////////////////////
	std::shared_ptr<Timer> _conTimer;
	struct sockaddr _peerAddr;
	spin_mutex _mtx_read;
	spin_mutex _mtx_err;
	spin_mutex _mtx_accept;
	spin_mutex _mtx_flush;
	onReadCB _readCB;
	onErrCB _errCB;
	onAcceptCB _acceptCB;
	onFlush _flushCB;
	Ticker _flushTicker;
    int _lastSendFlags = TCP_DEFAULE_FLAGS;
    uint32_t _iMaxSendPktSize = MAX_SEND_PKT;
    atomic_bool _enableRecv;

	void closeSock();
	bool setPeerSock(int fd, struct sockaddr *addr);
	bool attachEvent(const SockFD::Ptr &pSock,bool isUdp = false);

	int onAccept(const SockFD::Ptr &pSock,int event);
	int onRead(const SockFD::Ptr &pSock,bool mayEof=true);
	void onError(const SockFD::Ptr &pSock);
	int realSend(const string &buf, struct sockaddr *peerAddr,int flags);
	int onWrite(const SockFD::Ptr &pSock, bool bMainThread,int flags,bool isUdp);
	void onConnected(const SockFD::Ptr &pSock, const onErrCB &connectCB);
	void onFlushed(const SockFD::Ptr &pSock);

	void startWriteEvent(const SockFD::Ptr &pSock);
	void stopWriteEvent(const SockFD::Ptr &pSock);
	bool sendTimeout(bool isUdp);
	SockFD::Ptr makeSock(int sock){
		return std::make_shared<SockFD>(sock);
	}
	static SockException getSockErr(const SockFD::Ptr &pSock,bool tryErrno=true);

};

}  // namespace Network
}  // namespace ZL

#endif /* Socket_h */
