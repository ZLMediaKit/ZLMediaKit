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

#include <signal.h>
#include <string.h>
#include <stdlib.h>
#include "mediakit.h"
#ifdef _WIN32
#include "windows.h"
#else
#include "unistd.h"
#endif

#define LOG_LEV 4

typedef struct {
    mk_tcp_session *_session;
    //下面你可以夹杂你的私货数据
    char your_some_useful_data[1024];
} websocket_user_data;
/**
 * 当websocket客户端连接服务器时触发
 * @param session 会话处理对象
 */
void API_CALL on_mk_websocket_session_create(mk_tcp_session session){
    log_printf(LOG_LEV,"%s %d",mk_tcp_session_peer_ip(session),(int)mk_tcp_session_peer_port(session));
    websocket_user_data *user_data = malloc(sizeof(websocket_user_data));
    mk_websocket_session_set_user_data(session,user_data);
}

/**
 * session会话对象销毁时触发
 * 请在本回调中清理释放你的用户数据
 * 本事件中不能调用mk_tcp_session_send/mk_tcp_session_send_safe函数
 * @param session 会话处理对象
 */
void API_CALL on_mk_websocket_session_destory(mk_tcp_session session){
    log_printf(LOG_LEV,"%s %d",mk_tcp_session_peer_ip(session),(int)mk_tcp_session_peer_port(session));
    websocket_user_data *user_data = (websocket_user_data *)mk_websocket_session_get_user_data(session);
    free(user_data);
}

/**
 * 收到websocket客户端发过来的数据
 * @param session 会话处理对象
 * @param data 数据指针
 * @param len 数据长度
 */
void API_CALL on_mk_websocket_session_data(mk_tcp_session session,const char *data,int len){
    log_printf(LOG_LEV,"from %s %d, data[%d]: %s",mk_tcp_session_peer_ip(session),(int)mk_tcp_session_peer_port(session), len,data);
    mk_tcp_session_send(session,"echo:",0);
    mk_tcp_session_send(session,data,len);
}

/**
 * 每隔2秒的定时器，用于管理超时等任务
 * @param session 会话处理对象
 */
void API_CALL on_mk_websocket_session_manager(mk_tcp_session session){
    log_printf(LOG_LEV,"%s %d",mk_tcp_session_peer_ip(session),(int)mk_tcp_session_peer_port(session));
}

/**
 * on_mk_websocket_session_destory之前触发on_mk_websocket_session_err
 * 一般由于客户端断开tcp触发
 * 本事件中可以调用mk_tcp_session_send_safe函数
 * @param session 会话处理对象
 * @param code 错误代码
 * @param msg 错误提示
 */
void API_CALL on_mk_websocket_session_err(mk_tcp_session session,int code,const char *msg){
    log_printf(LOG_LEV,"%s %d will destory: %d %s",mk_tcp_session_peer_ip(session),(int)mk_tcp_session_peer_port(session),code,msg);
}

static int flag = 1;
static void on_exit(int sig){
    flag = 0;
}
int main(int argc, char *argv[]) {
    char ini_path[2048] = {0};
    strcpy(ini_path,argv[0]);
    strcat(ini_path,".ini");

    mk_env_init1(0, 0, 1, ini_path, 0, NULL, NULL);
    mk_websocket_events events = {
            .on_mk_websocket_session_create = on_mk_websocket_session_create,
            .on_mk_websocket_session_destory = on_mk_websocket_session_destory,
            .on_mk_websocket_session_data = on_mk_websocket_session_data,
            .on_mk_websocket_session_manager = on_mk_websocket_session_manager,
            .on_mk_websocket_session_err = on_mk_websocket_session_err
    };

    mk_websocket_events_listen(&events);
    mk_websocket_server_start(80,0);

    signal(SIGINT, on_exit );// 设置退出信号
    while (flag) {
#ifdef _WIN32
        Sleep(1000);
#else
        sleep(1);
#endif
    }
    mk_stop_all_server();
    return 0;
}