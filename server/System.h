//
// Created by xzl on 2018/9/5.
//

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
