/*
 * Copyright (c) 2016 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/xia-chu/ZLMediaKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#ifndef ZLMEDIAKIT_SYSTEM_H
#define ZLMEDIAKIT_SYSTEM_H

#include <string>

class System {
public:
    static std::string execute(const std::string &cmd);
    static void startDaemon(bool &kill_parent_if_failed);
    static void systemSetup();
};

#endif //ZLMEDIAKIT_SYSTEM_H
