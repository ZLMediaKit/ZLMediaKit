/*
 * MIT License
 *
 * Copyright (c) 2019 xiongziliang <771730766@qq.com>
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

#include <csignal>
#include "mediakit.h"
#ifdef _WIN32
#include "windows.h"
#else
#include "unistd.h"
#endif
int main(int argc,char *argv[]){
    mk_env_init1(0,0,0, nullptr,0, nullptr, nullptr);
    mk_http_server_start(80,false);
    mk_rtsp_server_start(554,false);
    mk_rtmp_server_start(1935,false);
    mk_rtp_server_start(10000);
    static bool flag = true;
    signal(SIGINT, [](int) { flag = false; });// 设置退出信号
    while (flag){
#ifdef _WIN32
        Sleep(1000);
#else
        sleep(1);
#endif
    }
    mk_stop_all_server();
    return 0;
}