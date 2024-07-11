﻿/*
 * Copyright (c) 2016-present The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/ZLMediaKit/ZLMediaKit).
 *
 * Use of this source code is governed by MIT-like license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#include <limits.h>
#include <sys/stat.h>
#ifndef _WIN32
#include <sys/resource.h>
#include <unistd.h>
#else
#include <io.h>
#include <windows.h>
#endif

#include <csignal>
#include <stdexcept>
#include "Process.h"
#include "Util/File.h"
#include "Util/util.h"
#include "Util/logger.h"
#include "Util/uv_errno.h"
#include "Poller/EventPoller.h"

#define STACK_SIZE (8192 * 1024)

using namespace std;
using namespace toolkit;

#ifndef _WIN32

static void setupChildProcess() {
    //取消cpu亲和性设置，防止FFmpeg进程cpu占用率不能超过100%的问题
    setThreadAffinity(-1);
    //子进程关闭core文件生成
    struct rlimit rlim = { 0, 0 };
    setrlimit(RLIMIT_CORE, &rlim);
    //子进程恢复默认信号处理
    signal(SIGINT, SIG_DFL);
    signal(SIGTERM, SIG_DFL);
    signal(SIGSEGV, SIG_DFL);
    signal(SIGABRT, SIG_DFL);
}

/* Start function for cloned child */
static int runChildProcess(string cmd, string log_file) {
    setupChildProcess();

    if (log_file.empty()) {
        //未指定子进程日志文件时，重定向至/dev/null
        log_file = "/dev/null";
    } else {
        log_file = StrPrinter << log_file << "." << getpid();
    }

    if (isatty(STDIN_FILENO)) {
        /* bb_error_msg("ignoring input"); */
        close(STDIN_FILENO);
        open("/dev/null", O_RDONLY, 0666); /* will be fd 0 (STDIN_FILENO) */
    }

    //重定向shell日志至文件
    auto fp = File::create_file(log_file, "ab");
    if (!fp) {
        fprintf(stderr, "open log file %s failed:%d(%s)\r\n", log_file.data(), get_uv_error(), get_uv_errmsg());
    } else {
        auto log_fd = fileno(fp);
        // dup to stdout and stderr.
        if (dup2(log_fd, STDOUT_FILENO) < 0) {
            fprintf(stderr, "dup2 stdout file %s failed:%d(%s)\r\n", log_file.data(), get_uv_error(), get_uv_errmsg());
        }
        if (dup2(log_fd, STDERR_FILENO) < 0) {
            fprintf(stderr, "dup2 stderr file  %s failed:%d(%s)\r\n", log_file.data(), get_uv_error(), get_uv_errmsg());
        }
        // 关闭日志文件
        ::fclose(fp);
    }
    fprintf(stderr, "\r\n\r\n#### pid=%d,cmd=%s #####\r\n\r\n", getpid(), cmd.data());

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
    delete[] charpv_params;

    if (ret < 0) {
        fprintf(stderr, "execv process failed:%d(%s)\r\n", get_uv_error(), get_uv_errmsg());
    }
    return ret;
}

static int cloneFunc(void *ptr) {
    auto pair = reinterpret_cast<std::pair<string, string> *>(ptr);
    return runChildProcess(pair->first, pair->second);
}

#endif

void Process::run(const string &cmd, string log_file) {
    kill(2000);
#ifdef _WIN32
    STARTUPINFO si = { 0 };
    PROCESS_INFORMATION pi = { 0 };
    if (log_file.empty()) {
        //未指定子进程日志文件时，重定向至/dev/null
        log_file = "NUL";
    } else {
        log_file = StrPrinter << log_file << "." << getCurrentMillisecond();
    }

    //重定向shell日志至文件
    auto fp = File::create_file(log_file, "ab");
    if (!fp) {
        fprintf(stderr, "open log file %s failed:%d(%s)\r\n", log_file.data(), get_uv_error(), get_uv_errmsg());
    } else {
        auto log_fd = (HANDLE)(_get_osfhandle(_fileno(fp)));
        // dup to stdout and stderr.
        si.wShowWindow = SW_HIDE;
        // STARTF_USESHOWWINDOW:The wShowWindow member contains additional information.
        // STARTF_USESTDHANDLES:The hStdInput, hStdOutput, and hStdError members contain additional information.
        si.dwFlags = STARTF_USESHOWWINDOW | STARTF_USESTDHANDLES;
        si.hStdError = log_fd;
        si.hStdOutput = log_fd;
    }

    LPTSTR lpDir = const_cast<char *>(cmd.data());
    if (CreateProcess(NULL, lpDir, NULL, NULL, TRUE, 0, NULL, NULL, &si, &pi)) {
        //下面两行关闭句柄，解除本进程和新进程的关系，不然有可能 不小心调用TerminateProcess函数关掉子进程
        CloseHandle(pi.hThread);
        _pid = pi.dwProcessId;
        _handle = pi.hProcess;
        fprintf(fp, "\r\n\r\n#### pid=%d,cmd=%s #####\r\n\r\n", _pid, cmd.data());
        InfoL << "start child process " << _pid << ", log file:" << log_file;
    } else {
        WarnL << "start child process fail: " << get_uv_errmsg();
    }
    fclose(fp);
#else

#if (defined(__linux) || defined(__linux__))
    _process_stack = malloc(STACK_SIZE);
    auto args = std::make_pair(cmd, log_file);
    _pid = clone(reinterpret_cast<int (*)(void *)>(&cloneFunc), (char *)_process_stack + STACK_SIZE, CLONE_FS | SIGCHLD, (void *)(&args));
    if (_pid == -1) {
        WarnL << "clone process failed:" << get_uv_errmsg();
        free(_process_stack);
        _process_stack = nullptr;
        throw std::runtime_error(StrPrinter << "clone child process failed, cmd: " << cmd << ",err:" << get_uv_errmsg());
    }
#else
    _pid = fork();
    if (_pid == -1) {
        throw std::runtime_error(StrPrinter << "fork child process failed, cmd: " << cmd << ",err:" << get_uv_errmsg());
    }
    if (_pid == 0) {
        //子进程
        exit(runChildProcess(cmd, log_file));
    }
#endif
    if (log_file.empty()) {
        //未指定子进程日志文件时，重定向至/dev/null
        log_file = "/dev/null";
    } else {
        log_file = StrPrinter << log_file << "." << _pid;
    }
    InfoL << "start child process " << _pid << ", log file:" << log_file;
#endif // _WIN32
}

/**
 * 获取进程是否存活状态
 * @param pid 进程号
 * @param exit_code_ptr 进程返回代码
 * @param block 是否阻塞等待
 * @return 进程是否还在运行
 */
static bool s_wait(pid_t pid, void *handle, int *exit_code_ptr, bool block) {
    if (pid <= 0) {
        return false;
    }
#ifdef _WIN32
    DWORD code = 0;
    if (block) {
        //一直等待
        code = WaitForSingleObject(handle, INFINITE);
    } else {
        code = WaitForSingleObject(handle, 0);
    }

    if (code == WAIT_FAILED || code == WAIT_OBJECT_0) {
        //子进程已经退出了,获取子进程退出代码
        DWORD exitCode = 0;
        if (exit_code_ptr && GetExitCodeProcess(handle, &exitCode)) {
            *exit_code_ptr = exitCode;
        }
        return false;
    }

    if (code == WAIT_TIMEOUT) {
        //子进程还在线
        return true;
    }
    //不太可能运行到此处
    WarnL << "WaitForSingleObject ret:" << code;
    return false;
#else
    int status = 0;
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
    return true;
#endif // _WIN32
}

#ifdef _WIN32
// Inspired from http://stackoverflow.com/a/15281070/1529139
// and http://stackoverflow.com/q/40059902/1529139
bool signalCtrl(DWORD dwProcessId, DWORD dwCtrlEvent) {
    bool success = false;
    DWORD thisConsoleId = GetCurrentProcessId();
    // Leave current console if it exists
    // (otherwise AttachConsole will return ERROR_ACCESS_DENIED)
    bool consoleDetached = (FreeConsole() != FALSE);

    if (AttachConsole(dwProcessId) != FALSE) {
        // Add a fake Ctrl-C handler for avoid instant kill is this console
        // WARNING: do not revert it or current program will be also killed
        SetConsoleCtrlHandler(nullptr, true);
        success = (GenerateConsoleCtrlEvent(dwCtrlEvent, 0) != FALSE);
        FreeConsole();
    }

    if (consoleDetached) {
        // Create a new console if previous was deleted by OS
        if (AttachConsole(thisConsoleId) == FALSE) {
            int errorCode = GetLastError();
            if (errorCode == 31) {
                // 31=ERROR_GEN_FAILURE
                AllocConsole();
            }
        }
    }
    return success;
}
#endif // _WIN32

static void s_kill(pid_t pid, void *handle, int max_delay, bool force) {
    if (pid <= 0) {
        // pid无效
        return;
    }
#ifdef _WIN32
    // windows下目前没有比较好的手段往子进程发送SIGTERM或信号
    //所以杀死子进程的方式全部强制为立即关闭
    force = true;
    if (force) {
        //强制关闭子进程
        TerminateProcess(handle, 0);
    } else {
        //非强制关闭，发送Ctr+C信号
        signalCtrl(pid, CTRL_C_EVENT);
    }
#else
    if (::kill(pid, force ? SIGKILL : SIGTERM) == -1) {
        //进程可能已经退出了
        WarnL << "kill process " << pid << " failed:" << get_uv_errmsg();
        return;
    }
#endif // _WIN32

    if (force) {
        //发送SIGKILL信号后，阻塞等待退出
        s_wait(pid, handle, nullptr, true);
        DebugL << "force kill " << pid << " success!";
        return;
    }

    //发送SIGTERM信号后，2秒后检查子进程是否已经退出
    EventPollerPool::Instance().getPoller()->doDelayTask(max_delay, [pid, handle]() {
        if (!s_wait(pid, handle, nullptr, false)) {
            //进程已经退出了
            return 0;
        }
        //进程还在运行
        WarnL << "process still working,force kill it:" << pid;
        s_kill(pid, handle, 0, true);
        return 0;
    });
}

void Process::kill(int max_delay, bool force) {
    if (_pid <= 0) {
        return;
    }
    s_kill(_pid, _handle, max_delay, force);
    _pid = -1;
#ifdef _WIN32
    if (_handle) {
        CloseHandle(_handle);
        _handle = nullptr;
    }
#elif ((defined(__linux) || defined(__linux__)))
    if (_process_stack) {
        free(_process_stack);
        _process_stack = nullptr;
    }
#endif
}

Process::~Process() {
    kill(2000);
}

Process::Process() {}

bool Process::wait(bool block) {
    return s_wait(_pid, _handle, &_exit_code, block);
}

int Process::exit_code() {
    return _exit_code;
}
