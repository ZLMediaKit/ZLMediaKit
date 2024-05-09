/*
 * Copyright (c) 2016-present The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/ZLMediaKit/ZLMediaKit).
 *
 * Use of this source code is governed by MIT-like license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#ifndef ZLMEDIAKIT_MACROS_H
#define ZLMEDIAKIT_MACROS_H

#include "Util/logger.h"
#include <iostream>
#include <sstream>
#if defined(__MACH__)
#include <arpa/inet.h>
#include <machine/endian.h>
#define __BYTE_ORDER BYTE_ORDER
#define __BIG_ENDIAN BIG_ENDIAN
#define __LITTLE_ENDIAN LITTLE_ENDIAN
#elif defined(__linux__)
#include <arpa/inet.h>
#include <endian.h>
#elif defined(_WIN32)
#define BIG_ENDIAN 1
#define LITTLE_ENDIAN 0
#define BYTE_ORDER LITTLE_ENDIAN
#define __BYTE_ORDER BYTE_ORDER
#define __BIG_ENDIAN BIG_ENDIAN
#define __LITTLE_ENDIAN LITTLE_ENDIAN
#endif

#ifndef CHECK
#define CHECK(exp, ...) ::mediakit::Assert_ThrowCpp(!(exp), #exp, __FUNCTION__, __FILE__, __LINE__, ##__VA_ARGS__)
#endif // CHECK

#ifndef CHECK_RET
#define CHECK_RET(...)                                                         \
    try {                                                                      \
        CHECK(__VA_ARGS__);                                                    \
    } catch (AssertFailedException & ex) {                                     \
        WarnL << ex.what();                                                    \
        return;                                                                \
    }
#endif

#ifndef MAX
#define MAX(a, b) ((a) > (b) ? (a) : (b))
#endif // MAX

#ifndef MIN
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#endif // MIN

#ifndef CLEAR_ARR
#define CLEAR_ARR(arr)                                                                                                 \
    for (auto &item : arr) {                                                                                           \
        item = 0;                                                                                                      \
    }
#endif // CLEAR_ARR

#define RTSP_SCHEMA "rtsp"
#define RTMP_SCHEMA "rtmp"
#define TS_SCHEMA "ts"
#define FMP4_SCHEMA "fmp4"
#define HLS_SCHEMA "hls"
#define HLS_FMP4_SCHEMA "hls.fmp4"

#define VHOST_KEY "vhost"
#define DEFAULT_VHOST "__defaultVhost__"

#ifdef __cplusplus
extern "C" {
#endif
extern void Assert_Throw(int failed, const char *exp, const char *func, const char *file, int line, const char *str);
#ifdef __cplusplus
}
#endif

namespace mediakit {

class AssertFailedException : public std::runtime_error {
public:
    template<typename ...T>
    AssertFailedException(T && ...args) : std::runtime_error(std::forward<T>(args)...) {}
};

extern const char kServerName[];

template <typename... ARGS>
void Assert_ThrowCpp(int failed, const char *exp, const char *func, const char *file, int line, ARGS &&...args) {
    if (failed) {
        std::stringstream ss;
        toolkit::LoggerWrapper::appendLog(ss, std::forward<ARGS>(args)...);
        Assert_Throw(failed, exp, func, file, line, ss.str().data());
    }
}

} // namespace mediakit
#endif // ZLMEDIAKIT_MACROS_H
