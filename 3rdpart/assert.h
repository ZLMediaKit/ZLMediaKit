/*
 * Copyright (c) 2016 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/xiongziliang/ZLMediaKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#ifndef ZLMEDIAKIT_ASSERT_H
#define ZLMEDIAKIT_ASSERT_H

#include <stdio.h>
#ifndef NDEBUG
    #ifdef assert
        #undef assert
    #endif//assert

    #ifdef __cplusplus
    extern "C" {
    #endif
    extern void Assert_Throw(int failed, const char *exp, const char *func, const char *file, int line);
    #ifdef __cplusplus
    }
    #endif

    #define assert(exp) Assert_Throw(!(exp), #exp, __FUNCTION__, __FILE__, __LINE__);
#else
    #define	assert(e)	((void)0)
#endif//NDEBUG

#endif //ZLMEDIAKIT_ASSERT_H
