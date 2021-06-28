/*
 * Copyright (c) 2016 The ZLToolKit project authors. All Rights Reserved.
 *
 * This file is part of ZLToolKit(https://github.com/xia-chu/ZLToolKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#include <iostream>
#include "Util/logger.h"
#include "Rtcp/RtcpFCI.h"
using namespace std;
using namespace toolkit;
using namespace mediakit;

extern void testFCI();

int main() {
    Logger::Instance().add(std::make_shared<ConsoleChannel>());

    testFCI();
    return 0;
}
