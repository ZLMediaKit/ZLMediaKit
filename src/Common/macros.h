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


#endif //ZLMEDIAKIT_MACROS_H
