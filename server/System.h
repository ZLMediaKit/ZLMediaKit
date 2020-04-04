/*
 * Copyright (c) 2016 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/xiongziliang/ZLMediaKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#ifndef ZLMEDIAKIT_SYSTEM_H
#define ZLMEDIAKIT_SYSTEM_H

#include <string>
using namespace std;

class System {
public:
    static string execute(const string &cmd);
    static void startDaemon();
    static void systemSetup();
};

#endif //ZLMEDIAKIT_SYSTEM_H
