/**
ISC License

Copyright © 2015, Iñaki Baz Castillo <ibc@aliax.net>

Permission to use, copy, modify, and/or distribute this software for any
purpose with or without fee is hereby granted, provided that the above
copyright notice and this permission notice appear in all copies.

THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */
 
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

}// namespace Utils

#endif
