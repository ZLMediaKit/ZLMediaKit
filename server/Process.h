/*
 * Copyright (c) 2016 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/xia-chu/ZLMediaKit).
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

class Process {
public:
    Process();
    ~Process();
    void run(const std::string &cmd, std::string &log_file);
    void kill(int max_delay,bool force = false);
    bool wait(bool block = true);
    int exit_code();
private:
    int _exit_code = 0;
    pid_t _pid = -1;
    void *_handle = nullptr;
#if (defined(__linux) || defined(__linux__))
    void *_process_stack = nullptr;
#endif
};


#endif //ZLMEDIAKIT_PROCESS_H
