﻿/*
 * Copyright (c) 2016-present The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/ZLMediaKit/ZLMediaKit).
 *
 * Use of this source code is governed by MIT-like license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#if !defined(_WIN32)
#include <limits.h>
#include <sys/resource.h>
#include <sys/wait.h>
#if !defined(ANDROID)
#include <execinfo.h>
#endif//!defined(ANDROID)
#endif//!defined(_WIN32)

#include <cstdlib>
#include <csignal>
#include <map>
#include <iostream>

#include "System.h"
#include "Util/util.h"
#include "Util/logger.h"
#include "Util/uv_errno.h"
#include "Common/macros.h"
#include "Common/JemallocUtil.h"

using namespace std;
using namespace toolkit;
using namespace mediakit;

#ifdef _WIN32
#define popen _popen
#define pclose _pclose
#endif

string System::execute(const string &cmd) {
    FILE *fPipe = NULL;
    fPipe = popen(cmd.data(), "r");
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

#if !defined(ANDROID) && !defined(_WIN32)

static constexpr int MAX_STACK_FRAMES = 128;

static void save_jemalloc_stats() {
    string jemalloc_status = JemallocUtil::get_malloc_stats();
    if (jemalloc_status.empty()) {
        return;
    }
    ofstream out(StrPrinter << exeDir() << "/jemalloc.json", ios::out | ios::binary | ios::trunc);
    out << jemalloc_status;
    out.flush();
}

static std::string get_func_symbol(const std::string &symbol) {
    size_t pos1 = symbol.find("(");
    if (pos1 == string::npos) {
        return "";
    }
    size_t pos2 = symbol.find("+", pos1);
    auto ret = symbol.substr(pos1 + 1, pos2 - pos1 - 1);
    return ret;
}

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
        auto func_symbol = get_func_symbol(symbol);
        if (!func_symbol.empty()) {
            ref.emplace_back(toolkit::demangle(func_symbol.data()));
        }
        static auto addr2line = [](const string &address) {
            string cmd = StrPrinter << "addr2line -C -f -e " << exePath() << " " << address;
            return System::execute(cmd);
        };
        size_t pos1 = symbol.find_first_of("[");
        size_t pos2 = symbol.find_last_of("]");
        std::string address = symbol.substr(pos1 + 1, pos2 - pos1 - 1);
        ref.emplace_back(addr2line(address));
#endif//__linux
    }
    free(strings);

    stringstream ss;
    ss << "## crash date:" << getTimeStr("%Y-%m-%d %H:%M:%S") << endl;
    ss << "## exe:       " << exeName() << endl;
    ss << "## signal:    " << sig << endl;
    ss << "## version:   " << kServerName << endl;
    ss << "## stack:     " << endl;
    for (size_t i = 0; i < stack.size(); ++i) {
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
}
#endif // !defined(ANDROID) && !defined(_WIN32)


void System::startDaemon(bool &kill_parent_if_failed) {
    kill_parent_if_failed = true;
#ifndef _WIN32
    static pid_t pid;
    do {
        pid = fork();
        if (pid == -1) {
            WarnL << "fork失败:" << get_uv_errmsg();
            //休眠1秒再试
            sleep(1);
            continue;
        }

        if (pid == 0) {
            //子进程
            return;
        }

        //父进程,监视子进程是否退出
        DebugL << "启动子进程:" << pid;
        signal(SIGINT, [](int) {
            WarnL << "收到主动退出信号,关闭父进程与子进程";
            kill(pid, SIGINT);
            exit(0);
        });

        signal(SIGTERM,[](int) {
            WarnL << "收到主动退出信号,关闭父进程与子进程";
            kill(pid, SIGINT);
            exit(0);
        });

        do {
            int status = 0;
            if (waitpid(pid, &status, 0) >= 0) {
                WarnL << "子进程退出";
                //休眠3秒再启动子进程
                sleep(3);
                //重启子进程，如果子进程重启失败，那么不应该杀掉守护进程，这样守护进程可以一直尝试重启子进程
                kill_parent_if_failed = false;
                break;
            }
            DebugL << "waitpid被中断:" << get_uv_errmsg();
        } while (true);
    } while (true);
#endif // _WIN32
}

void System::systemSetup(){

#ifdef ENABLE_JEMALLOC_DUMP
    //Save memory report when program exits
    atexit(save_jemalloc_stats);
#endif //ENABLE_JEMALLOC_DUMP

#if !defined(_WIN32)
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
    //忽略挂起信号
    signal(SIGHUP, SIG_IGN);
#endif// ANDROID
#endif//!defined(_WIN32)
}

