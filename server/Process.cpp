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

#include <limits.h>
#include <sys/resource.h>
#include <unistd.h>
#include <stdexcept>
#include <signal.h>
#include "Util/util.h"
#include "Util/File.h"
#include "Util/logger.h"
#include "Util/uv_errno.h"
#include "Util/TimeTicker.h"
#include "Process.h"
#include "Poller/Timer.h"
using namespace toolkit;

void Process::run(const string &cmd, const string &log_file_tmp) {
    kill(2000);
    _pid = fork();
    if (_pid < 0) {
        throw std::runtime_error(StrPrinter << "fork child process falied,err:" << get_uv_errmsg());
    }
    if (_pid == 0) {
        //子进程

        //子进程关闭core文件生成
        struct rlimit rlim = {0,0};
        setrlimit(RLIMIT_CORE, &rlim);

        // ignore the SIGINT and SIGTERM
        signal(SIGINT, SIG_IGN);
        signal(SIGTERM, SIG_IGN);

        string log_file ;
        if(log_file_tmp.empty()){
            log_file = "/dev/null";
        }else{
            log_file = StrPrinter << log_file_tmp << "." << getpid();
        }

        int log_fd = -1;
        int flags = O_CREAT | O_WRONLY | O_APPEND;
        mode_t mode = S_IRWXO | S_IRWXG | S_IRWXU;// S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH;
        File::createfile_path(log_file.data(), mode);
        if ((log_fd = ::open(log_file.c_str(), flags, mode)) < 0) {
            fprintf(stderr, "open log file %s failed:%d(%s)\r\n", log_file.data(), errno, strerror(errno));
        } else {
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
        for (int i = 0; i < (int) params.size(); i++) {
            std::string &p = params[i];
            charpv_params[i] = (char *) p.data();
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
}

void Process::kill(int max_delay) {
    if (_pid <= 0) {
        return;
    }
    if (::kill(_pid, SIGTERM) == -1) {
        WarnL << "kill process " << _pid << " falied,err:" << get_uv_errmsg();
    } else {
        //等待子进程退出
        auto pid = _pid;
        EventPollerPool::Instance().getPoller()->doDelayTask(max_delay,[pid](){
            //最多等待２秒，２秒后强制杀掉程序
            if (waitpid(pid, NULL, WNOHANG) == 0) {
                ::kill(pid, SIGKILL);
                WarnL << "force kill process " << pid;
            }
            return 0;
        });
    }
    _pid = -1;
}

Process::~Process() {
    kill(2000);
}

Process::Process() {
}

bool Process::wait(bool block) {
    if (_pid <= 0) {
        return false;
    }
    int status = 0;
    pid_t p = waitpid(_pid, &status, block ? 0 : WNOHANG);

    _exit_code = (status & 0xFF00) >> 8;
    if (p < 0) {
        WarnL << "waitpid failed, pid=" << _pid << ", err=" << get_uv_errmsg();
        return false;
    }
    if (p > 0) {
        InfoL << "process terminated, pid=" << _pid << ", exit code=" << _exit_code;
        return false;
    }

    //WarnL << "process is running, pid=" << _pid;
    return true;
}

int Process::exit_code() {
    return _exit_code;
}
