/*
 * Copyright (c) 2016 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/xiongziliang/ZLMediaKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#ifndef ZLMEDIAKIT_PROCESS_H
#define ZLMEDIAKIT_PROCESS_H

#ifdef _WIN32
typedef int pid_t;
#else
#include <sys/wait.h>
#endif // _WIN32

#include <fcntl.h>
#include <string>
using namespace std;

class Process {
public:
    Process();
    ~Process();
    void run(const string &cmd,const string &log_file);
    void kill(int max_delay,bool force = false);
    bool wait(bool block = true);
    int exit_code();
private:
    pid_t _pid = -1;
    int _exit_code = 0;
};


#endif //ZLMEDIAKIT_PROCESS_H
