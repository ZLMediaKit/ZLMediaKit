/*
 * MIT License
 *
 * Copyright (c) 2016-2019 xiongziliang <771730766@qq.com>
 *
 * This file is part of ZLMediaKit(https://github.com/xiongziliang/ZLMediaKit).
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include "System.h"
#include <stdlib.h>
#include <signal.h>
#include <arpa/inet.h>
#include <limits.h>
#include <sys/resource.h>
#include <unistd.h>
#include <sys/wait.h>
#ifndef ANDROID
#include <execinfo.h>
#endif
#include <map>
#include <string>
#include <iostream>
#include "Util/mini.h"
#include "Util/util.h"
#include "Util/logger.h"
#include "Util/onceToken.h"
#include "Util/NoticeCenter.h"
#include "System.h"
#include "Util/uv_errno.h"
#include "Util/CMD.h"
#include "Util/MD5.h"
using namespace toolkit;

const int MAX_STACK_FRAMES = 128;
#define BroadcastOnCrashDumpArgs int &sig,const vector<vector<string> > &stack
const char kBroadcastOnCrashDump[] = "kBroadcastOnCrashDump";

//#if defined(__MACH__) || defined(__APPLE__)
//#define TEST_LINUX
//#endif

vector<string> splitWithEmptyLine(const string &s, const char *delim) {
    vector<string> ret;
    int last = 0;
    int index = s.find(delim, last);
    while (index != string::npos) {
        ret.push_back(s.substr(last, index - last));
        last = index + strlen(delim);
        index = s.find(delim, last);
    }
    if (s.size() - last > 0) {
        ret.push_back(s.substr(last));
    }
    return ret;
}

map<string, mINI> splitTopStr(const string &cmd_str) {
    map<string, mINI> ret;
    auto lines = splitWithEmptyLine(cmd_str, "\n");
    int i = 0;
    for (auto &line : lines) {
        if(i++ < 1 && line.empty()){
            continue;
        }
        if (line.empty()) {
            break;
        }
        auto line_vec = split(line, ":");

        if (line_vec.size() < 2) {
            continue;
        }
        trim(line_vec[0], " \r\n\t");
        auto args_vec = split(line_vec[1], ",");
        for (auto &arg : args_vec) {
            auto arg_vec = split(trim(arg, " \r\n\t."), " ");
            if (arg_vec.size() < 2) {
                continue;
            }
            ret[line_vec[0]].emplace(arg_vec[1], arg_vec[0]);
        }
    }
    return ret;
}

bool System::getSystemUsage(SystemUsage &usage) {
    try {
#if defined(__linux) || defined(__linux__) || defined(TEST_LINUX)
        string cmd_str;
#if !defined(TEST_LINUX)
        cmd_str = System::execute("top -b -n 1");
#else
        cmd_str = "top - 07:21:31 up  5:48,  2 users,  load average: 0.03, 0.62, 0.54\n"
                  "Tasks:  80 total,   1 running,  78 sleeping,   0 stopped,   1 zombie\n"
                  "%Cpu(s):  0.8 us,  0.4 sy,  0.0 ni, 98.8 id,  0.0 wa,  0.0 hi,  0.0 si,  0.0 st\n"
                  "KiB Mem:   2058500 total,   249100 used,  1809400 free,    19816 buffers\n"
                  "KiB Swap:  1046524 total,        0 used,  1046524 free.   153012 cached Mem\n"
                  "\n";
#endif
        if (cmd_str.empty()) {
            WarnL << "System::execute(\"top -b -n 1\") return empty";
            return false;
        }
        auto topMap = splitTopStr(cmd_str);

        usage.task_total = topMap["Tasks"]["total"];
        usage.task_running = topMap["Tasks"]["running"];
        usage.task_sleeping = topMap["Tasks"]["sleeping"];
        usage.task_stopped = topMap["Tasks"]["stopped"];

        usage.cpu_user = topMap["%Cpu(s)"]["us"];
        usage.cpu_sys = topMap["%Cpu(s)"]["sy"];
        usage.cpu_idle = topMap["%Cpu(s)"]["id"];

        usage.mem_total = topMap["KiB Mem"]["total"];
        usage.mem_free = topMap["KiB Mem"]["free"];
        usage.mem_used = topMap["KiB Mem"]["used"];
        return true;

#elif defined(__MACH__) || defined(__APPLE__)
        /*
        "Processes: 275 total, 2 running, 1 stuck, 272 sleeping, 1258 threads \n"
        "2018/09/12 10:41:32\n"
        "Load Avg: 2.06, 2.88, 2.86 \n"
        "CPU usage: 14.54% user, 25.45% sys, 60.0% idle \n"
        "SharedLibs: 117M resident, 37M data, 15M linkedit.\n"
        "MemRegions: 46648 total, 3654M resident, 62M private, 714M shared.\n"
        "PhysMem: 7809M used (1906M wired), 381M unused.\n"
        "VM: 751G vsize, 614M framework vsize, 0(0) swapins, 0(0) swapouts.\n"
        "Networks: packets: 502366/248M in, 408957/87M out.\n"
        "Disks: 349435/6037M read, 78622/2577M written.";
         */

        string cmd_str = System::execute("top -l 1");
        if(cmd_str.empty()){
            WarnL << "System::execute(\"top -n 1\") return empty";
            return false;
        }
        auto topMap = splitTopStr(cmd_str);
        usage.task_total = topMap["Processes"]["total"];
        usage.task_running = topMap["Processes"]["running"];
        usage.task_sleeping = topMap["Processes"]["sleeping"];
        usage.task_stopped = topMap["Processes"]["stuck"];

        usage.cpu_user = topMap["CPU usage"]["user"];
        usage.cpu_sys = topMap["CPU usage"]["sys"];
        usage.cpu_idle = topMap["CPU usage"]["idle"];

        usage.mem_free = topMap["PhysMem"]["unused"].as<uint32_t>() * 1024 * 1024;
        usage.mem_used = topMap["PhysMem"]["used"].as<uint32_t>() * 1024 * 1024;
        usage.mem_total = usage.mem_free + usage.mem_used;
        return true;
#else
        WarnL << "System not supported";
        return false;
#endif
    } catch (std::exception &ex) {
        WarnL << ex.what();
        return false;
    }
}

bool System::getNetworkUsage(vector<NetworkUsage> &usage) {
    try {
#if defined(__linux) || defined(__linux__) || defined(TEST_LINUX)
        string cmd_str;
#if !defined(TEST_LINUX)
        cmd_str = System::execute("cat /proc/net/dev");
#else
        cmd_str =
                "Inter-|   Receive                                                |  Transmit\n"
                " face |bytes    packets errs drop fifo frame compressed multicast|bytes    packets errs drop fifo colls carrier compressed\n"
                "    lo:  475978    7546    0    0    0     0          0         0   475978    7546    0    0    0     0       0          0\n"
                "enp3s0: 151747818  315136    0    0    0     0          0       145 1590783447 1124616    0    0    0     0       0          0";
#endif
        if (cmd_str.empty()) {
            return false;
        }
        auto lines = split(cmd_str, "\n");
        int i = 0;
        vector<string> column_name_vec;
        vector<string> category_prefix_vec;
        for (auto &line : lines) {
            switch (i++) {
                case 0: {
                    category_prefix_vec = split(line, "|");
                }
                    break;
                case 1: {
                    auto category_suffix_vec = split(line, "|");
                    int j = 0;
                    for (auto &category_suffix : category_suffix_vec) {
                        auto column_suffix_vec = split(category_suffix, " ");
                        for (auto &column_suffix : column_suffix_vec) {
                            column_name_vec.emplace_back(trim(category_prefix_vec[j]) + "-" + trim(column_suffix));
                        }
                        j++;
                    }
                }
                    break;
                default: {
                    mINI valMap;
                    auto vals = split(line, " ");
                    int j = 0;
                    for (auto &val : vals) {
                        valMap[column_name_vec[j++]] = trim(val, " \r\n\t:");
                    }
                    usage.emplace_back(NetworkUsage());
                    auto &ifrUsage = usage.back();
                    ifrUsage.interface = valMap["Inter--face"];
                    ifrUsage.recv_bytes = valMap["Receive-bytes"];
                    ifrUsage.recv_packets = valMap["Receive-packets"];
                    ifrUsage.snd_bytes = valMap["Transmit-bytes"];
                    ifrUsage.snd_packets = valMap["Transmit-packets"];
                }
                    break;
            }
        }
        return true;
#else
        WarnL << "System not supported";
        return false;
#endif
    } catch (std::exception &ex) {
        WarnL << ex.what();
        return false;
    }
}


bool System::getTcpUsage(System::TcpUsage &usage) {
    usage.established =  atoi(trim(System::execute("netstat -na|grep ESTABLISHED|wc -l")).data());
    usage.syn_recv =  atoi(trim(System::execute("netstat -na|grep SYN_RECV|wc -l")).data());
    usage.time_wait =  atoi(trim(System::execute("netstat -na|grep TIME_WAIT|wc -l")).data());
    usage.close_wait =  atoi(trim(System::execute("netstat -na|grep CLOSE_WAIT|wc -l")).data());
    return true;
}

string System::execute(const string &cmd) {
//    DebugL << cmd;
    FILE *fPipe = popen(cmd.data(), "r");
    if(!fPipe){
        return "";
    }
    string ret;
    char buff[1024] = {0};
    while(fgets(buff, sizeof(buff) - 1, fPipe)){
        ret.append(buff);
    }
    pclose(fPipe);
    return ret;
}

static string addr2line(const string &address) {
    string cmd = StrPrinter << "addr2line -e " << exePath() << " " << address;
    return System::execute(cmd);
}

#ifndef ANDROID
static void sig_crash(int sig) {
    signal(sig, SIG_DFL);
    void *array[MAX_STACK_FRAMES];
    int size = backtrace(array, MAX_STACK_FRAMES);
    char ** strings = backtrace_symbols(array, size);
    vector<vector<string> > stack(size);

    for (int i = 0; i < size; ++i) {
        auto &ref = stack[i];
        std::string symbol(strings[i]);
        ref.emplace_back(symbol);
#if defined(__linux) || defined(__linux__)
        size_t pos1 = symbol.find_first_of("[");
        size_t pos2 = symbol.find_last_of("]");
        std::string address = symbol.substr(pos1 + 1, pos2 - pos1 - 1);
        ref.emplace_back(addr2line(address));
#endif//__linux
    }
    free(strings);

    NoticeCenter::Instance().emitEvent(kBroadcastOnCrashDump,sig,stack);
}

#endif//#ifndef ANDROID


void System::startDaemon() {
    static pid_t pid;
    do{
        pid = fork();
        if(pid == -1){
            WarnL << "fork失败:" << get_uv_errmsg();
            //休眠1秒再试
            sleep(1);
            continue;
        }

        if(pid == 0){
            //子进程
            return;
        }

        //父进程,监视子进程是否退出
        DebugL << "启动子进程:"  << pid;
        signal(SIGINT, [](int) {
            WarnL << "收到主动退出信号,关闭父进程与子进程";
            kill(pid,SIGINT);
            exit(0);
        });

        do{
            int status = 0;
            if(waitpid(pid, &status, 0) >= 0) {
                WarnL << "子进程退出";
                //休眠1秒再启动子进程
                sleep(1);
                break;
            }
            DebugL << "waitpid被中断:" << get_uv_errmsg();
        }while (true);
    }while (true);
}



static string currentDateTime(){
    time_t ts = time(NULL);
    std::tm tm_snapshot;
    localtime_r(&ts, &tm_snapshot);

    char buffer[1024] = {0};
    std::strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", &tm_snapshot);
    return buffer;
}

void System::systemSetup(){
    struct rlimit rlim,rlim_new;
    if (getrlimit(RLIMIT_CORE, &rlim)==0) {
        rlim_new.rlim_cur = rlim_new.rlim_max = RLIM_INFINITY;
        if (setrlimit(RLIMIT_CORE, &rlim_new)!=0) {
            rlim_new.rlim_cur = rlim_new.rlim_max = rlim.rlim_max;
            setrlimit(RLIMIT_CORE, &rlim_new);
        }
        InfoL << "core文件大小设置为:" << rlim_new.rlim_cur;
    }

    if (getrlimit(RLIMIT_NOFILE, &rlim)==0) {
        rlim_new.rlim_cur = rlim_new.rlim_max = RLIM_INFINITY;
        if (setrlimit(RLIMIT_NOFILE, &rlim_new)!=0) {
            rlim_new.rlim_cur = rlim_new.rlim_max = rlim.rlim_max;
            setrlimit(RLIMIT_NOFILE, &rlim_new);
        }
        InfoL << "文件最大描述符个数设置为:" << rlim_new.rlim_cur;
    }

#ifndef ANDROID
    signal(SIGSEGV, sig_crash);
    signal(SIGABRT, sig_crash);
#endif//#ifndef ANDROID

    NoticeCenter::Instance().addListener(nullptr,kBroadcastOnCrashDump,[](BroadcastOnCrashDumpArgs){
        stringstream ss;
        ss << "## crash date:" << currentDateTime() << endl;
        ss << "## exe:       " << exeName() << endl;
        ss << "## signal:    " << sig << endl;
        ss << "## stack:     " << endl;
        for (int i = 0; i < stack.size(); ++i) {
            ss << "[" << i << "]: ";
            for (auto &str : stack[i]){
                ss << str << endl;
            }
        }
        string stack_info = ss.str();
        ofstream out(StrPrinter << exeDir() << "/crash." << getpid(), ios::out | ios::binary | ios::trunc);
        out << stack_info;
        out.flush();

        cerr << stack_info << endl;
    });
}

