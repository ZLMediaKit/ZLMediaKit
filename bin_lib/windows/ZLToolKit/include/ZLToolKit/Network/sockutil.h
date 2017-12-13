#ifndef SOCKUTIL_H
#define SOCKUTIL_H

#if defined(_WIN32)
#include <WinSock2.h>
#include <Iphlpapi.h>
#pragma comment (lib,"WS2_32")
#pragma comment(lib,"Iphlpapi.lib") 
#else
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <net/if.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#endif // defined(_WIN32)

#include <string>
#include <string.h>
#include <stdint.h>

using namespace std;

#if defined(_WIN32)

#ifndef socklen_t
#define socklen_t int
#endif //!socklen_t
#ifndef SHUT_RDWR
#define SHUT_RDWR 2
#endif //!SHUT_RDWR

int ioctl(int fd, long cmd, u_long *ptr);
int close(int fd);

#endif // defined(_WIN32)

namespace ZL {
namespace Network {

class SockUtil {
public:
	static int connect(const char *host, uint16_t port, bool bAsync = true);
	static int listen(const uint16_t port, const char *localIp = "0.0.0.0",
			int backLog = 1024);
	static int bindUdpSock(const uint16_t port,
			const char *localIp = "0.0.0.0");
	static int setNoDelay(int sockFd, bool on = true);
	static int setNoSigpipe(int sock);
	static int setNoBlocked(int sock, bool noblock = true);
	static int setRecvBuf(int sock, int size = 256 * 1024);
	static int setSendBuf(int sock, int size = 256 * 1024);

	static int setReuseable(int sockFd, bool on = true);
	static int setBroadcast(int sockFd, bool on = true);
	static int setKeepAlive(int sockFd, bool on = true);

	//组播相关
	static int setMultiTTL(int sockFd, uint8_t ttl = 64);
	static int setMultiIF(int sockFd, const char *strLocalIp);
	static int setMultiLOOP(int sockFd, bool bAccept = false);
	static int joinMultiAddr(int sockFd, const char *strAddr, const char* strLocalIp = "0.0.0.0");
	static int leaveMultiAddr(int sockFd, const char *strAddr, const char* strLocalIp = "0.0.0.0");
	static int joinMultiAddrFilter(int sockFd, const char* strAddr, const char* strSrcIp, const char* strLocalIp = "0.0.0.0");
	static int leaveMultiAddrFilter(int sockFd, const char* strAddr, const char* strSrcIp, const char* strLocalIp = "0.0.0.0");

	static int getSockError(int sockFd);
	static int setCloseWait(int sockFd, int second = 0);

	static string get_local_ip(int fd);
	static string get_local_ip();
	static uint16_t get_local_port(int fd);
	static string get_peer_ip(int fd);
	static uint16_t get_peer_port(int fd);

	static string get_ifr_name(const char *localIp);
	static string get_ifr_mask(const char *ifrName);
	static string get_ifr_brdaddr(const char *ifrName);
	static bool in_same_lan(const char *myIp, const char *dsrIp);
};

}  // namespace Network
}  // namespace ZL

#endif // !SOCKUTIL_H
