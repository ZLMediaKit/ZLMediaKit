#pragma once

#include "../../QuicPluginABI.h"

#include <lsquic.h>
#include <openssl/err.h>
#include <openssl/ssl.h>

#include <memory>
#include <string>

#if defined(_WIN32)
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#endif

namespace mediakit {
namespace lsquicplugin {

QuicSlice makeSlice(const char *str);
QuicSlice makeSlice(const std::string &str);
std::string toString(QuicSlice slice);
std::string toErrorString(int err);

bool acquireRuntime();
void releaseRuntime();

bool sliceToSockaddr(QuicSlice ip, uint16_t port, sockaddr_storage &storage, socklen_t &len);
bool sockaddrToEndpoint(const sockaddr *addr, std::string &ip, uint16_t &port);

using SslCtxPtr = std::unique_ptr<SSL_CTX, decltype(&SSL_CTX_free)>;

} // namespace lsquicplugin
} // namespace mediakit
