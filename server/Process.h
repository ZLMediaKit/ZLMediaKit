//
// Created by xzl on 2018/5/24.
//

#ifndef IPTV_PROCESS_H
#define IPTV_PROCESS_H

#include <sys/wait.h>
#include <sys/fcntl.h>
#include <string>
using namespace std;

class Process {
public:
    Process();
    ~Process();
    void run(const string &cmd,const string &log_file);
    void kill(int max_delay);
    bool wait(bool block = true);
    int exit_code();
private:
    pid_t _pid = -1;
    int _exit_code = 0;
};


#endif //IPTV_PROCESS_H
