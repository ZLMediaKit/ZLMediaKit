/*
 * Copyright (c) 2016 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/xia-chu/ZLMediaKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#ifndef ZLMEDIAKIT_MACROS_H
#define ZLMEDIAKIT_MACROS_H

#if defined(ENABLE_VERSION)
#include "Version.h"
#endif

#if defined(__MACH__)
#include <arpa/inet.h>
    #include <machine/endian.h>
    #define __BYTE_ORDER BYTE_ORDER
    #define __BIG_ENDIAN BIG_ENDIAN
    #define __LITTLE_ENDIAN LITTLE_ENDIAN
#elif defined(__linux__)
    #include <endian.h>
    #include <arpa/inet.h>
#elif defined(_WIN32)
    #define BIG_ENDIAN 1
    #define LITTLE_ENDIAN 0
    #define BYTE_ORDER LITTLE_ENDIAN
    #define __BYTE_ORDER BYTE_ORDER
    #define __BIG_ENDIAN BIG_ENDIAN
    #define __LITTLE_ENDIAN LITTLE_ENDIAN
#endif

#ifndef PACKED
    #if !defined(_WIN32)
        #define PACKED    __attribute__((packed))
    #else
        #define PACKED
    #endif //!defined(_WIN32)
#endif

#ifndef CHECK
#define CHECK(exp) Assert_Throw(!(exp), #exp, __FUNCTION__, __FILE__, __LINE__)
#endif//CHECK

#ifndef MAX
#define MAX(a,b) ((a) > (b) ? (a) : (b) )
#endif //MAX

#ifndef MIN
#define MIN(a,b) ((a) < (b) ? (a) : (b) )
#endif //MIN

#ifndef CLEAR_ARR
#define CLEAR_ARR(arr) for(auto &item : arr){ item = 0;}
#endif //CLEAR_ARR

//请遵循MIT协议，勿修改服务器声明
#if !defined(ENABLE_VERSION)
#define SERVER_NAME "ZLMediaKit-6.0(build in " __DATE__ " " __TIME__ ")"
#else
#define SERVER_NAME "ZLMediaKit(git hash:" COMMIT_HASH ",branch:" BRANCH_TIME ",build time:" __DATE__ " " __TIME__ ")"
#endif

#define VHOST_KEY "vhost"
#define HTTP_SCHEMA "http"
#define RTSP_SCHEMA "rtsp"
#define RTMP_SCHEMA "rtmp"
#define HLS_SCHEMA "hls"
#define TS_SCHEMA "ts"
#define FMP4_SCHEMA "fmp4"
#define DEFAULT_VHOST "__defaultVhost__"

#ifdef __cplusplus
extern "C" {
#endif
extern void Assert_Throw(int failed, const char *exp, const char *func, const char *file, int line);
#ifdef __cplusplus
}
#endif

#endif //ZLMEDIAKIT_MACROS_H
