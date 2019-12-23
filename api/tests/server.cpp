//
// Created by xzl on 2019/12/23.
//

#include <csignal>
#include "mediakit.h"
#include "unistd.h"
int main(int argc,char *argv[]){
    mk_env_init1(0,0,0, nullptr,0, nullptr, nullptr);
    mk_http_server_start(80,false);
    mk_rtsp_server_start(554,false);
    mk_rtmp_server_start(1935,false);
    mk_rtp_server_start(10000);

    static bool flag = true;
    signal(SIGINT, [](int) { flag = false; });// 设置退出信号

    while (flag){
        sleep(1);
    }
}