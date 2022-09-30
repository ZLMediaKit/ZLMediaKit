#ifndef ZLMEDIAKIT_SRT_COMMON_H
#define ZLMEDIAKIT_SRT_COMMON_H
#if defined(_WIN32)
#include <winsock2.h>
#include <ws2tcpip.h>

#include <Iphlpapi.h>
#pragma comment(lib, "Ws2_32.lib")
#pragma comment(lib, "Iphlpapi.lib")
#else
#include <netdb.h>
#include <sys/socket.h>
#endif // defined(_WIN32)

#include <chrono>
#define MAX_SEQ  0x7fffffff
#define SEQ_NONE 0xffffffff
#define MAX_TS   0xffffffff

namespace SRT {
using SteadyClock = std::chrono::steady_clock;
using TimePoint = std::chrono::time_point<SteadyClock>;

using Microseconds = std::chrono::microseconds;
using Milliseconds = std::chrono::milliseconds;

static inline int64_t DurationCountMicroseconds(SteadyClock::duration dur) {
    return std::chrono::duration_cast<std::chrono::microseconds>(dur).count();
}

static inline uint32_t loadUint32(uint8_t *ptr) {
    return ptr[0] << 24 | ptr[1] << 16 | ptr[2] << 8 | ptr[3];
}

static inline uint16_t loadUint16(uint8_t *ptr) {
    return ptr[0] << 8 | ptr[1];
}

inline static int64_t seqCmp(uint32_t seq1, uint32_t seq2) {
    if(seq1 > seq2){
        if((seq1 - seq2) >(MAX_SEQ>>1)){
            return (int64_t)seq1 - (int64_t)(seq2+MAX_SEQ);
        }else{
            return (int64_t)seq1 - (int64_t)seq2;
        }
    }else{
        if((seq2-seq1) >(MAX_SEQ>>1)){
            return (int64_t)(seq1+MAX_SEQ) - (int64_t)seq2;
        }else{
            return (int64_t)seq1 - (int64_t)seq2;
        }
    }
}

inline static uint32_t incSeq(int32_t seq) {
    return (seq == MAX_SEQ) ? 0 : seq + 1;
}
static inline void storeUint32(uint8_t *buf, uint32_t val) {
    buf[0] = val >> 24;
    buf[1] = (val >> 16) & 0xff;
    buf[2] = (val >> 8) & 0xff;
    buf[3] = val & 0xff;
}

static inline void storeUint16(uint8_t *buf, uint16_t val) {
    buf[0] = (val >> 8) & 0xff;
    buf[1] = val & 0xff;
}

static inline void storeUint32LE(uint8_t *buf, uint32_t val) {
    buf[0] = val & 0xff;
    buf[1] = (val >> 8) & 0xff;
    buf[2] = (val >> 16) & 0xff;
    buf[3] = (val >> 24) & 0xff;
}

static inline void storeUint16LE(uint8_t *buf, uint16_t val) {
    buf[0] = val & 0xff;
    buf[1] = (val >> 8) & 0xff;
}

static inline uint32_t srtVersion(int major, int minor, int patch) {
    return patch + minor * 0x100 + major * 0x10000;
}
static inline uint32_t genExpectedSeq(uint32_t seq) {
    return MAX_SEQ & seq;
}

class UTicker {
public:
    UTicker() { _created = _begin = SteadyClock::now(); }
    ~UTicker() = default;

    /**
     * 获取创建时间，单位微妙
     */
    int64_t elapsedTime(TimePoint now) const { return DurationCountMicroseconds(now - _begin); }

    /**
     * 获取上次resetTime后至今的时间，单位毫秒
     */
    int64_t createdTime(TimePoint now) const { return DurationCountMicroseconds(now - _created); }

    /**
     * 重置计时器
     */
    void resetTime(TimePoint now) { _begin = now; }

private:
    TimePoint _begin;
    TimePoint _created;
};

} // namespace SRT

#endif // ZLMEDIAKIT_SRT_COMMON_H