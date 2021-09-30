/*
 * Copyright (c) 2016 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/xia-chu/ZLMediaKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#include "macros.h"

#if defined(ENABLE_VERSION)
#include "Version.h"
#endif

//请遵循MIT协议，勿修改服务器声明
#if !defined(ENABLE_VERSION)
const char SERVER_NAME[] =  "ZLMediaKit-6.0(build in " __DATE__ " " __TIME__ ")";
#else
const char SERVER_NAME[] = "ZLMediaKit(git hash:" COMMIT_HASH ",branch:" BRANCH_NAME ",build time:" __DATE__ " " __TIME__ ")";
#endif

