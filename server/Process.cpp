/*
 * Copyright (c) 2016 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/xiongziliang/ZLMediaKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#include <limits.h>
#include <sys/stat.h>

#ifndef _WIN32
#include <sys/resource.h>
#include <unistd.h>
#else
//#include <TlHelp32.h>
#include <windows.h>
#endif

#include <stdexcept>
#include <signal.h>
#include "Util/util.h"
#include "Util/File.h"
#include "Util/logger.h"
#include "Util/uv_errno.h"
#include "Thread/WorkThreadPool.h"
#include "Process.h"
using namespace toolkit;

void Process::run(const string &cmd, const string &log_file_tmp) {
    kill(2000);
#ifdef _WIN32
    STARTUPINFO si;
    PROCESS_INFORMATION pi;
    ZeroMemory(&si, sizeof(si));			//结构体初始化；
    ZeroMemory(&pi, sizeof(pi));

    LPTSTR lpDir = const_cast<char*>(cmd.data());

    if (CreateProcess(NULL, lpDir, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi)){
        //下面两行关闭句柄，解除本进程和新进程的关系，不然有可能 不小心调用TerminateProcess函数关掉子进程 
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);

        _pid = pi.dwProcessId;
        InfoL << "start child proces " << _pid;
    } else {
        WarnL << "start child proces fail: " << GetLastError();
    }
#else	
    _pid = fork();
    if (_pid < 0) {
        throw std::runtime_error(StrPrinter << "fork child process falied,err:" << get_uv_errmsg());
    }
    if (_pid == 0) {
        //子进程关闭core文件生成
        struct rlimit rlim = { 0,0 };
        setrlimit(RLIMIT_CORE, &rlim);

        //在启动子进程时，暂时禁用SIGINT、SIGTERM信号
        // ignore the SIGINT and SIGTERM
        signal(SIGINT, SIG_IGN);
        signal(SIGTERM, SIG_IGN);

        string log_file;
        if (log_file_tmp.empty()) {
            log_file = "/dev/null";
        }
        else {
            log_file = StrPrinter << log_file_tmp << "." << getpid();
        }

        int log_fd = -1;
        int flags = O_CREAT | O_WRONLY | O_APPEND;
        mode_t mode = S_IRWXO | S_IRWXG | S_IRWXU;// S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH;
        File::create_path(log_file.data(), mode);
        if ((log_fd = ::open(log_file.c_str(), flags, mode)) < 0) {
            fprintf(stderr, "open log file %s failed:%d(%s)\r\n", log_file.data(), errno, strerror(errno));
        }
        else {
            // dup to stdout and stderr.
            if (dup2(log_fd, STDOUT_FILENO) < 0) {
                fprintf(stderr, "dup2 stdout file %s failed:%d(%s)\r\n", log_file.data(), errno, strerror(errno));
            }
            if (dup2(log_fd, STDERR_FILENO) < 0) {
                fprintf(stderr, "dup2 stderr file  %s failed:%d(%s)\r\n", log_file.data(), errno, strerror(errno));
            }
            // close log fd
            ::close(log_fd);
        }
        fprintf(stderr, "\r\n\r\n#### pid=%d,cmd=%s #####\r\n\r\n", getpid(), cmd.data());

        // close other fds
        // TODO: do in right way.
        for (int i = 3; i < 1024; i++) {
            ::close(i);
        }

        auto params = split(cmd, " ");
        // memory leak in child process, it's ok.
        char **charpv_params = new char *[params.size() + 1];
        for (int i = 0; i < (int)params.size(); i++) {
            std::string &p = params[i];
            charpv_params[i] = (char *)p.data();
        }
        // EOF: NULL
        charpv_params[params.size()] = NULL;

        // TODO: execv or execvp
        auto ret = execv(params[0].c_str(), charpv_params);
        if (ret < 0) {
            fprintf(stderr, "fork process failed, errno=%d(%s)\r\n", errno, strerror(errno));
        }
        exit(ret);
    }
    InfoL << "start child proces " << _pid;
#endif // _WIN32
}

/**
 * 获取进程是否存活状态
 * @param pid 进程号
 * @param exit_code_ptr 进程返回代码
 * @param block 是否阻塞等待
 * @return 进程是否还在运行
 */
static bool s_wait(pid_t pid,int *exit_code_ptr,bool block) {
    if (pid <= 0) {
        return false;
    }
    int status = 0;
#ifdef _WIN32
    HANDLE hProcess = NULL;
    hProcess = OpenProcess(PROCESS_TERMINATE, FALSE, pid);	//打开目标进程
    if (hProcess == NULL) {
        return false;
    }

    CloseHandle(hProcess);
#else
    pid_t p = waitpid(pid, &status, block ? 0 : WNOHANG);
    int exit_code = (status & 0xFF00) >> 8;
    if (exit_code_ptr) {
        *exit_code_ptr = exit_code;
    }
    if (p < 0) {
        WarnL << "waitpid failed, pid=" << pid << ", err=" << get_uv_errmsg();
        return false;
    }
    if (p > 0) {
        InfoL << "process terminated, pid=" << pid << ", exit code=" << exit_code;
        return false;
    }
#endif // _WIN32

    return true;
}

static void s_kill(pid_t pid,int max_delay,bool force){
    if (pid <= 0) {
        //pid无效
        return;
    }
#ifdef _WIN32
    HANDLE hProcess = NULL;
    hProcess = OpenProcess(PROCESS_TERMINATE, FALSE, pid);	//打开目标进程
    if (hProcess == NULL) {
        WarnL << "\nOpen Process fAiled: " << GetLastError();
        return;
    }
    DWORD ret = TerminateProcess(hProcess, 0);	//结束目标进程
    if (ret == 0) {
        WarnL << GetLastError;
    }
#else
    if (::kill(pid, force ? SIGKILL : SIGTERM) == -1) {
        //进程可能已经退出了
        WarnL << "kill process " << pid << " failed:" << get_uv_errmsg();
        return;
    }
#endif // _WIN32


    if(force){
        //发送SIGKILL信号后，阻塞等待退出
        s_wait(pid, NULL, true);
        DebugL << "force kill " << pid << " success!";
        return;
    }

    //发送SIGTERM信号后，2秒后检查子进程是否已经退出
    WorkThreadPool::Instance().getPoller()->doDelayTask(max_delay,[pid](){
        if (!s_wait(pid, nullptr, false)) {
            //进程已经退出了
            return 0;
        }
        //进程还在运行
        WarnL << "process still working,force kill it:" << pid;
        s_kill(pid,0, true);
        return 0;
    });
}

void Process::kill(int max_delay,bool force) {
    if (_pid <= 0) {
        return;
    }
    s_kill(_pid,max_delay,force);
    _pid = -1;
}

Process::~Process() {
    kill(2000);
}

Process::Process() {}

bool Process::wait(bool block) {
    return s_wait(_pid,&_exit_code,block);
}

int Process::exit_code() {
    return _exit_code;
}
