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

#ifndef IPTV_BASH_H
#define IPTV_BASH_H

#include <cstdint>
#include <string>
#include <vector>
#include <map>
#include "Util/NoticeCenter.h"

using namespace std;
using namespace toolkit;

class System {
public:
    typedef struct {
        uint32_t task_total;
        uint32_t task_running;
        uint32_t task_sleeping;
        uint32_t task_stopped;

        uint64_t mem_total;
        uint64_t mem_free;
        uint64_t mem_used;

        float cpu_user;
        float cpu_sys;
        float cpu_idle;
    } SystemUsage;

    typedef struct {
        uint64_t recv_bytes;
        uint64_t recv_packets;
        uint64_t snd_bytes;
        uint64_t snd_packets;
        string interface;
    } NetworkUsage;

    typedef struct {
        uint64_t available;
        uint64_t used;
        float used_per;
        string mounted_on;
        string filesystem;
        bool mounted;
    } DiskUsage;

    typedef struct {
        uint32_t established;
        uint32_t syn_recv;
        uint32_t time_wait;
        uint32_t close_wait;
    } TcpUsage;

    static bool getSystemUsage(SystemUsage &usage);
    static bool getNetworkUsage(vector<NetworkUsage> &usage);
    static bool getTcpUsage(TcpUsage &usage);
    static string execute(const string &cmd);
    static void startDaemon();
    static void systemSetup();

};


#endif //IPTV_BASH_H
