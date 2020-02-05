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

#if !defined(_WIN32)
#include <limits.h>
#include <sys/resource.h>
#include <sys/wait.h>
#if !defined(ANDROID)
#include <execinfo.h>
#endif//!defined(ANDROID)
#endif//!defined(_WIN32)

#include "System.h"
#include <signal.h>
#include <map>
#include <iostream>
#include "Util/logger.h"
#include "Util/NoticeCenter.h"
#include "Util/uv_errno.h"
using namespace toolkit;

const int MAX_STACK_FRAMES = 128;
#define BroadcastOnCrashDumpArgs int &sig,const vector<vector<string> > &stack
const char kBroadcastOnCrashDump[] = "kBroadcastOnCrashDump";

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
static string addr2line(const string &address) {
    string cmd = StrPrinter << "addr2line -e " << exePath() << " " << address;
    return System::execute(cmd);
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
        size_t pos1 = symbol.find_first_of("[");
        size_t pos2 = symbol.find_last_of("]");
        std::string address = symbol.substr(pos1 + 1, pos2 - pos1 - 1);
        ref.emplace_back(addr2line(address));
#endif//__linux
    }
    free(strings);
    NoticeCenter::Instance().emitEvent(kBroadcastOnCrashDump,sig,stack);
}
#endif // !defined(ANDROID) && !defined(_WIN32)


void System::startDaemon() {
#ifndef _WIN32
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
#endif // _WIN32
}

void System::systemSetup(){
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
    NoticeCenter::Instance().addListener(nullptr,kBroadcastOnCrashDump,[](BroadcastOnCrashDumpArgs){
        stringstream ss;
        ss << "## crash date:" << getTimeStr("%Y-%m-%d %H:%M:%S") << endl;
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
#endif// ANDROID
#endif//!defined(_WIN32)
}

