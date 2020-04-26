/*
 * Copyright (c) 2016 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/xiongziliang/ZLMediaKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#include <signal.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "mk_mediakit.h"
#ifdef _WIN32
#include "windows.h"
#else
#include "unistd.h"
#endif

#define LOG_LEV 4
//修改此宏，可以选择协议类型
#define TCP_TYPE mk_type_ws

static int flag = 1;
static void s_on_exit(int sig){
    flag = 0;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////
typedef struct {
    mk_tcp_session _session;
    //下面你可以夹杂你的私货数据
    char your_some_useful_data[1024];
} tcp_session_user_data;
/**
 * 当tcp客户端连接服务器时触发
 * @param session 会话处理对象
 */
void API_CALL on_mk_tcp_session_create(uint16_t server_port,mk_tcp_session session){
    char ip[64];
    log_printf(LOG_LEV,"%s %d",mk_tcp_session_peer_ip(session,ip),(int)mk_tcp_session_peer_port(session));
    tcp_session_user_data *user_data = malloc(sizeof(tcp_session_user_data));
    user_data->_session = session;
    mk_tcp_session_set_user_data(session,user_data);
}

/**
 * 收到tcp客户端发过来的数据
 * @param session 会话处理对象
 * @param data 数据指针
 * @param len 数据长度
 */
void API_CALL on_mk_tcp_session_data(uint16_t server_port,mk_tcp_session session,const char *data,int len){
    char ip[64];
    log_printf(LOG_LEV,"from %s %d, data[%d]: %s",mk_tcp_session_peer_ip(session,ip),(int)mk_tcp_session_peer_port(session), len,data);
    mk_tcp_session_send(session,"echo:",0);
    mk_tcp_session_send(session,data,len);
}

/**
 * 每隔2秒的定时器，用于管理超时等任务
 * @param session 会话处理对象
 */
void API_CALL on_mk_tcp_session_manager(uint16_t server_port,mk_tcp_session session){
    char ip[64];
    log_printf(LOG_LEV,"%s %d",mk_tcp_session_peer_ip(session,ip),(int)mk_tcp_session_peer_port(session));
}

/**
 * 一般由于客户端断开tcp触发
 * 本事件中可以调用mk_tcp_session_send_safe函数
 * @param session 会话处理对象
 * @param code 错误代码
 * @param msg 错误提示
 */
void API_CALL on_mk_tcp_session_disconnect(uint16_t server_port,mk_tcp_session session,int code,const char *msg){
    char ip[64];
    log_printf(LOG_LEV,"%s %d: %d %s",mk_tcp_session_peer_ip(session,ip),(int)mk_tcp_session_peer_port(session),code,msg);
    tcp_session_user_data *user_data = (tcp_session_user_data *)mk_tcp_session_get_user_data(session);
    free(user_data);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////
typedef struct {
    mk_tcp_client client;
    //下面你可以夹杂你的私货数据
    char your_some_useful_data[1024];
    int count;
} tcp_client_user_data;

/**
 * tcp客户端连接服务器成功或失败回调
 * @param client tcp客户端
 * @param code 0为连接成功，否则为失败原因
 * @param msg 连接失败错误提示
 */
void API_CALL on_mk_tcp_client_connect(mk_tcp_client client,int code,const char *msg){
    log_printf(LOG_LEV,"connect result:%d %s",code,msg);
    if(code == 0){
        //连接上后我们发送一个hello world测试数据
        mk_tcp_client_send(client,"hello world",0);
    }else{
        tcp_client_user_data *user_data = mk_tcp_client_get_user_data(client);
        mk_tcp_client_release(client);
        free(user_data);
    }
}

/**
 * tcp客户端与tcp服务器之间断开回调
 * 一般是eof事件导致
 * @param client tcp客户端
 * @param code 错误代码
 * @param msg 错误提示
 */
void API_CALL on_mk_tcp_client_disconnect(mk_tcp_client client,int code,const char *msg){
    log_printf(LOG_LEV,"disconnect:%d %s",code,msg);
    //服务器主动断开了，我们销毁对象
    tcp_client_user_data *user_data = mk_tcp_client_get_user_data(client);
    mk_tcp_client_release(client);
    free(user_data);
}

/**
 * 收到tcp服务器发来的数据
 * @param client tcp客户端
 * @param data 数据指针
 * @param len 数据长度
 */
void API_CALL on_mk_tcp_client_data(mk_tcp_client client,const char *data,int len){
    log_printf(LOG_LEV,"data[%d]:%s",len,data);
}

/**
 * 每隔2秒的定时器，用于管理超时等任务
 * @param client tcp客户端
 */
void API_CALL on_mk_tcp_client_manager(mk_tcp_client client){
    tcp_client_user_data *user_data = mk_tcp_client_get_user_data(client);
    char buf[1024];
    sprintf(buf,"on_mk_tcp_client_manager:%d",user_data->count);
    mk_tcp_client_send(client,buf,0);

    if(++user_data->count == 5){
        //发送5遍后主动释放对象
        mk_tcp_client_release(client);
        free(user_data);
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////
void test_server(){
    mk_tcp_session_events events_server = {
            .on_mk_tcp_session_create = on_mk_tcp_session_create,
            .on_mk_tcp_session_data = on_mk_tcp_session_data,
            .on_mk_tcp_session_manager = on_mk_tcp_session_manager,
            .on_mk_tcp_session_disconnect = on_mk_tcp_session_disconnect
    };

    mk_tcp_server_events_listen(&events_server);
    mk_tcp_server_start(80, TCP_TYPE);
}

void test_client(){
    mk_tcp_client_events events_clent = {
            .on_mk_tcp_client_connect = on_mk_tcp_client_connect,
            .on_mk_tcp_client_data = on_mk_tcp_client_data,
            .on_mk_tcp_client_disconnect = on_mk_tcp_client_disconnect,
            .on_mk_tcp_client_manager = on_mk_tcp_client_manager,
    };
    mk_tcp_client client = mk_tcp_client_create(&events_clent, TCP_TYPE);

    tcp_client_user_data *user_data = (tcp_client_user_data *)malloc(sizeof(tcp_client_user_data));
    user_data->client = client;
    user_data->count = 0;
    mk_tcp_client_set_user_data(client,user_data);

    mk_tcp_client_connect(client, "121.40.165.18", 8800, 3);
    //你可以连接127.0.0.1 80测试
//    mk_tcp_client_connect(client, "127.0.0.1", 80, 3);
}

int main(int argc, char *argv[]) {
    char *ini_path = mk_util_get_exe_dir("c_api.ini");
    char *ssl_path = mk_util_get_exe_dir("ssl.p12");

    mk_config config = {
            .ini = ini_path,
            .ini_is_path = 1,
            .log_level = 0,
            .ssl = ssl_path,
            .ssl_is_path = 1,
            .ssl_pwd = NULL,
            .thread_num = 0
    };
    mk_env_init(&config);
    free(ini_path);
    free(ssl_path);

    test_server();
    test_client();

    signal(SIGINT, s_on_exit );// 设置退出信号
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