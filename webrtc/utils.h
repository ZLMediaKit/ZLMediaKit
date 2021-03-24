#ifndef MS_UTILS_HPP
#define MS_UTILS_HPP

#if defined(_WIN32)
#include <winsock2.h>
#include <ws2tcpip.h>
#include <Iphlpapi.h>
#pragma comment (lib, "Ws2_32.lib")
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

#include <algorithm>// std::transform(), std::find(), std::min(), std::max()
#include <cinttypes>// PRIu64, etc
#include <cmath>
#include <cstddef>// size_t
#include <cstdint>// uint8_t, etc
#include <cstring>// std::memcmp(), std::memcpy()
#include <memory>
#include <openssl/bio.h>
#include <openssl/ssl.h>
#include <openssl/x509.h>
#include <string>

namespace Utils {
class IP {
public:
    static int GetFamily(const char *ip, size_t ipLen);
    static int GetFamily(const std::string &ip);
    static void GetAddressInfo(const struct sockaddr *addr, int &family, std::string &ip,
                               uint16_t &port);
    static bool CompareAddresses(const struct sockaddr *addr1, const struct sockaddr *addr2);
    static struct sockaddr_storage CopyAddress(const struct sockaddr *addr);
    static void NormalizeIp(std::string &ip);
};

/* Inline static methods. */

inline int IP::GetFamily(const std::string &ip) { return GetFamily(ip.c_str(), ip.size()); }

inline bool IP::CompareAddresses(const struct sockaddr *addr1, const struct sockaddr *addr2) {
    // Compare family.
    if (addr1->sa_family != addr2->sa_family ||
        (addr1->sa_family != AF_INET && addr1->sa_family != AF_INET6)) {
        return false;
    }

    // Compare port.
    if (reinterpret_cast<const struct sockaddr_in *>(addr1)->sin_port !=
        reinterpret_cast<const struct sockaddr_in *>(addr2)->sin_port) {
        return false;
    }

    // Compare IP.
    switch (addr1->sa_family) {
        case AF_INET: {
            return (reinterpret_cast<const struct sockaddr_in *>(addr1)->sin_addr.s_addr ==
                    reinterpret_cast<const struct sockaddr_in *>(addr2)->sin_addr.s_addr);
        }

        case AF_INET6: {
            return (std::memcmp(
                            std::addressof(reinterpret_cast<const struct sockaddr_in6 *>(addr1)->sin6_addr),
                            std::addressof(reinterpret_cast<const struct sockaddr_in6 *>(addr2)->sin6_addr),
                            16) == 0
                            ? true
                            : false);
        }

        default: {
            return false;
        }
    }
}

inline struct sockaddr_storage IP::CopyAddress(const struct sockaddr *addr) {
    struct sockaddr_storage copiedAddr;

    switch (addr->sa_family) {
        case AF_INET:
            std::memcpy(std::addressof(copiedAddr), addr, sizeof(struct sockaddr_in));
            break;

        case AF_INET6:
            std::memcpy(std::addressof(copiedAddr), addr, sizeof(struct sockaddr_in6));
            break;
    }

    return copiedAddr;
}

class File {
public:
    static void CheckFile(const char *file);
};

class Byte {
public:
    /**
     * Getters below get value in Host Byte Order.
     * Setters below set value in Network Byte Order.
     */
    static uint8_t Get1Byte(const uint8_t *data, size_t i);
    static uint16_t Get2Bytes(const uint8_t *data, size_t i);
    static uint32_t Get3Bytes(const uint8_t *data, size_t i);
    static uint32_t Get4Bytes(const uint8_t *data, size_t i);
    static uint64_t Get8Bytes(const uint8_t *data, size_t i);
    static void Set1Byte(uint8_t *data, size_t i, uint8_t value);
    static void Set2Bytes(uint8_t *data, size_t i, uint16_t value);
    static void Set3Bytes(uint8_t *data, size_t i, uint32_t value);
    static void Set4Bytes(uint8_t *data, size_t i, uint32_t value);
    static void Set8Bytes(uint8_t *data, size_t i, uint64_t value);
    static uint16_t PadTo4Bytes(uint16_t size);
    static uint32_t PadTo4Bytes(uint32_t size);
};

/* Inline static methods. */

inline uint8_t Byte::Get1Byte(const uint8_t *data, size_t i) { return data[i]; }

inline uint16_t Byte::Get2Bytes(const uint8_t *data, size_t i) {
    return uint16_t{data[i + 1]} | uint16_t{data[i]} << 8;
}

inline uint32_t Byte::Get3Bytes(const uint8_t *data, size_t i) {
    return uint32_t{data[i + 2]} | uint32_t{data[i + 1]} << 8 | uint32_t{data[i]} << 16;
}

inline uint32_t Byte::Get4Bytes(const uint8_t *data, size_t i) {
    return uint32_t{data[i + 3]} | uint32_t{data[i + 2]} << 8 | uint32_t{data[i + 1]} << 16 |
           uint32_t{data[i]} << 24;
}

inline uint64_t Byte::Get8Bytes(const uint8_t *data, size_t i) {
    return uint64_t{Byte::Get4Bytes(data, i)} << 32 | Byte::Get4Bytes(data, i + 4);
}

inline void Byte::Set1Byte(uint8_t *data, size_t i, uint8_t value) { data[i] = value; }

inline void Byte::Set2Bytes(uint8_t *data, size_t i, uint16_t value) {
    data[i + 1] = static_cast<uint8_t>(value);
    data[i] = static_cast<uint8_t>(value >> 8);
}

inline void Byte::Set3Bytes(uint8_t *data, size_t i, uint32_t value) {
    data[i + 2] = static_cast<uint8_t>(value);
    data[i + 1] = static_cast<uint8_t>(value >> 8);
    data[i] = static_cast<uint8_t>(value >> 16);
}

inline void Byte::Set4Bytes(uint8_t *data, size_t i, uint32_t value) {
    data[i + 3] = static_cast<uint8_t>(value);
    data[i + 2] = static_cast<uint8_t>(value >> 8);
    data[i + 1] = static_cast<uint8_t>(value >> 16);
    data[i] = static_cast<uint8_t>(value >> 24);
}

inline void Byte::Set8Bytes(uint8_t *data, size_t i, uint64_t value) {
    data[i + 7] = static_cast<uint8_t>(value);
    data[i + 6] = static_cast<uint8_t>(value >> 8);
    data[i + 5] = static_cast<uint8_t>(value >> 16);
    data[i + 4] = static_cast<uint8_t>(value >> 24);
    data[i + 3] = static_cast<uint8_t>(value >> 32);
    data[i + 2] = static_cast<uint8_t>(value >> 40);
    data[i + 1] = static_cast<uint8_t>(value >> 48);
    data[i] = static_cast<uint8_t>(value >> 56);
}

inline uint16_t Byte::PadTo4Bytes(uint16_t size) {
    // If size is not multiple of 32 bits then pad it.
    if (size & 0x03)
        return (size & 0xFFFC) + 4;
    else
        return size;
}

inline uint32_t Byte::PadTo4Bytes(uint32_t size) {
    // If size is not multiple of 32 bits then pad it.
    if (size & 0x03)
        return (size & 0xFFFFFFFC) + 4;
    else
        return size;
}

class Bits {
public:
    static size_t CountSetBits(const uint16_t mask);
};

/* Inline static methods. */

class Crypto {
public:
    static void ClassInit();
    static void ClassDestroy();
    static uint32_t GetRandomUInt(uint32_t min, uint32_t max);
    static const std::string GetRandomString(size_t len);
    static uint32_t GetCRC32(const uint8_t *data, size_t size);
    static const uint8_t *GetHmacShA1(const std::string &key, const uint8_t *data, size_t len);

private:
    static uint32_t seed;
    static HMAC_CTX *hmacSha1Ctx;
    static uint8_t hmacSha1Buffer[];
    static const uint32_t crc32Table[256];
};

/* Inline static methods. */

inline uint32_t Crypto::GetRandomUInt(uint32_t min, uint32_t max) {
    // NOTE: This is the original, but produces very small values.
    // Crypto::seed = (214013 * Crypto::seed) + 2531011;
    // return (((Crypto::seed>>16)&0x7FFF) % (max - min + 1)) + min;

    // This seems to produce better results.
    Crypto::seed = uint32_t{((214013 * Crypto::seed) + 2531011)};

    return (((Crypto::seed >> 4) & 0x7FFF7FFF) % (max - min + 1)) + min;
}

inline const std::string Crypto::GetRandomString(size_t len) {
    static char buffer[64];
    static const char chars[] = {'0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'a', 'b',
                                 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n',
                                 'o', 'p', 'q', 'r', 's', 't', 'u', 'v', 'w', 'x', 'y', 'z'};

    if (len > 64) len = 64;

    for (size_t i{0}; i < len; ++i) {
        buffer[i] = chars[GetRandomUInt(0, sizeof(chars) - 1)];
    }

    return std::string(buffer, len);
}

inline uint32_t Crypto::GetCRC32(const uint8_t *data, size_t size) {
    uint32_t crc{0xFFFFFFFF};
    const uint8_t *p = data;

    while (size--) {
        crc = Crypto::crc32Table[(crc ^ *p++) & 0xFF] ^ (crc >> 8);
    }

    return crc ^ ~0U;
}

class String {
public:
    static void ToLowerCase(std::string &str);
};

inline void String::ToLowerCase(std::string &str) {
    std::transform(str.begin(), str.end(), str.begin(), ::tolower);
}

class Time {
    // Seconds from Jan 1, 1900 to Jan 1, 1970.
    static constexpr uint32_t UnixNtpOffset{0x83AA7E80};
    // NTP fractional unit.
    static constexpr uint64_t NtpFractionalUnit{1LL << 32};

public:
    struct Ntp {
        uint32_t seconds;
        uint32_t fractions;
    };

    static Time::Ntp TimeMs2Ntp(uint64_t ms);
    static uint64_t Ntp2TimeMs(Time::Ntp ntp);
    static bool IsNewerTimestamp(uint32_t timestamp, uint32_t prevTimestamp);
    static uint32_t LatestTimestamp(uint32_t timestamp1, uint32_t timestamp2);
};

inline Time::Ntp Time::TimeMs2Ntp(uint64_t ms) {
    Time::Ntp ntp;// NOLINT(cppcoreguidelines-pro-type-member-init)

    ntp.seconds = uint32_t(ms / 1000);
    ntp.fractions =
            static_cast<uint32_t>((static_cast<double>(ms % 1000) / 1000) * NtpFractionalUnit);

    return ntp;
}

inline uint64_t Time::Ntp2TimeMs(Time::Ntp ntp) {
    // clang-format off
    return (
        static_cast<uint64_t>(ntp.seconds) * 1000 +
        static_cast<uint64_t>(std::round((static_cast<double>(ntp.fractions) * 1000) / NtpFractionalUnit))
    );
    // clang-format on
}

inline bool Time::IsNewerTimestamp(uint32_t timestamp, uint32_t prevTimestamp) {
    // Distinguish between elements that are exactly 0x80000000 apart.
    // If t1>t2 and |t1-t2| = 0x80000000: IsNewer(t1,t2)=true,
    // IsNewer(t2,t1)=false
    // rather than having IsNewer(t1,t2) = IsNewer(t2,t1) = false.
    if (static_cast<uint32_t>(timestamp - prevTimestamp) == 0x80000000)
        return timestamp > prevTimestamp;

    return timestamp != prevTimestamp &&
           static_cast<uint32_t>(timestamp - prevTimestamp) < 0x80000000;
}

inline uint32_t Time::LatestTimestamp(uint32_t timestamp1, uint32_t timestamp2) {
    return IsNewerTimestamp(timestamp1, timestamp2) ? timestamp1 : timestamp2;
}

}// namespace Utils

#endif
