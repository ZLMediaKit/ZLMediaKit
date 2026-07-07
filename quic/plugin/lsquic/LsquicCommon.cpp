#include "LsquicCommon.h"

#include <cstring>
#include <mutex>

namespace mediakit {
namespace lsquicplugin {

QuicSlice makeSlice(const char *str) {
    QuicSlice ret;
    ret.data = str;
    ret.len = str ? std::char_traits<char>::length(str) : 0;
    return ret;
}

QuicSlice makeSlice(const std::string &str) {
    QuicSlice ret;
    ret.data = str.data();
    ret.len = str.size();
    return ret;
}

std::string toString(QuicSlice slice) {
    if (!slice.data || !slice.len) {
        return std::string();
    }
    return std::string(slice.data, slice.len);
}

std::string toErrorString(int err) {
    auto *msg = ERR_error_string(err, nullptr);
    return msg ? std::string(msg) : std::string("unknown error");
}

namespace {

std::mutex &lsquicRuntimeMutex() {
    static std::mutex s_mutex;
    return s_mutex;
}

size_t &lsquicRuntimeRefCount() {
    static size_t s_ref_count = 0;
    return s_ref_count;
}

} // namespace

bool acquireRuntime() {
    std::lock_guard<std::mutex> lock(lsquicRuntimeMutex());
    auto &ref_count = lsquicRuntimeRefCount();
    if (ref_count == 0) {
        // lsquic keeps process-global state, so the shared plugin ref-counts
        // initialization across all plugin instances created by the host.
        if (lsquic_global_init(LSQUIC_GLOBAL_CLIENT | LSQUIC_GLOBAL_SERVER) != 0) {
            return false;
        }
    }
    ++ref_count;
    return true;
}

void releaseRuntime() {
    std::lock_guard<std::mutex> lock(lsquicRuntimeMutex());
    auto &ref_count = lsquicRuntimeRefCount();
    if (ref_count == 0) {
        return;
    }
    if (--ref_count == 0) {
        lsquic_global_cleanup();
    }
}

bool sliceToSockaddr(QuicSlice ip, uint16_t port, sockaddr_storage &storage, socklen_t &len) {
    auto text = toString(ip);
    if (text.empty()) {
        return false;
    }

    std::memset(&storage, 0, sizeof(storage));
    sockaddr_in addr4 = {};
    addr4.sin_family = AF_INET;
    addr4.sin_port = htons(port);
    if (inet_pton(AF_INET, text.c_str(), &addr4.sin_addr) == 1) {
        std::memcpy(&storage, &addr4, sizeof(addr4));
        len = sizeof(addr4);
        return true;
    }

    sockaddr_in6 addr6 = {};
    addr6.sin6_family = AF_INET6;
    addr6.sin6_port = htons(port);
    if (inet_pton(AF_INET6, text.c_str(), &addr6.sin6_addr) == 1) {
        std::memcpy(&storage, &addr6, sizeof(addr6));
        len = sizeof(addr6);
        return true;
    }

    return false;
}

bool sockaddrToEndpoint(const sockaddr *addr, std::string &ip, uint16_t &port) {
    if (!addr) {
        return false;
    }

    char buf[INET6_ADDRSTRLEN] = {0};
    switch (addr->sa_family) {
    case AF_INET: {
        auto *addr4 = reinterpret_cast<const sockaddr_in *>(addr);
        if (!inet_ntop(AF_INET, &addr4->sin_addr, buf, sizeof(buf))) {
            return false;
        }
        ip = buf;
        port = ntohs(addr4->sin_port);
        return true;
    }
    case AF_INET6: {
        auto *addr6 = reinterpret_cast<const sockaddr_in6 *>(addr);
        if (!inet_ntop(AF_INET6, &addr6->sin6_addr, buf, sizeof(buf))) {
            return false;
        }
        ip = buf;
        port = ntohs(addr6->sin6_port);
        return true;
    }
    default:
        return false;
    }
}

} // namespace lsquicplugin
} // namespace mediakit
