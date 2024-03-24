/*
 * Copyright (c) 2016-present The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/ZLMediaKit/ZLMediaKit).
 *
 * Use of this source code is governed by MIT-like license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#include "macros.h"
#include "Util/util.h"

using namespace toolkit;

#if defined(ENABLE_VERSION)
#include "ZLMVersion.h"
#endif

extern "C" {
void Assert_Throw(int failed, const char *exp, const char *func, const char *file, int line, const char *str) {
    if (failed) {
        _StrPrinter printer;
        printer << "Assertion failed: (" << exp ;
        if(str && *str){
            printer << ", " << str;
        }
        printer << "), function " << func << ", file " << file << ", line " << line << ".";
        throw mediakit::AssertFailedException(printer);
    }
}
}

namespace mediakit {

/**
 * 本项目采用类MIT协议，用户在履行MIT协议义务的同时，应当同时遵循保留ZLMediaKit软件版权信息的义务。
 * 用户不得去除ZLMediaKit提供的各种服务中包括但不限于 "title"、"Server"、"User-Agent" 等字段中 "ZLMediaKit" 的信息。
 * 否则本项目主要权利人(项目发起人、主要作者)保留声索起诉的权利。
 */
#if !defined(ENABLE_VERSION)
const char kServerName[] =  "ZLMediaKit-8.0(build in " __DATE__ " " __TIME__ ")";
#else
const char kServerName[] = "ZLMediaKit(git hash:" COMMIT_HASH "/" COMMIT_TIME ",branch:" BRANCH_NAME ",build time:" BUILD_TIME ")";
#endif

}//namespace mediakit
