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
#include "Util/util.h"

using namespace toolkit;

#if defined(ENABLE_VERSION)
#include "version.h"
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
        throw std::runtime_error(printer);
    }
}
}

namespace mediakit {

//请遵循MIT协议，勿修改服务器声明
#if !defined(ENABLE_VERSION)
const char kServerName[] =  "ZLMediaKit-6.0(build in " __DATE__ " " __TIME__ ")";
#else
const char kServerName[] = "ZLMediaKit(git hash:" COMMIT_HASH ",branch:" BRANCH_NAME ",build time:" __DATE__ " " __TIME__ ")";
#endif

}//namespace mediakit