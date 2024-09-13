/*
 * Copyright (c) 2016-present The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/ZLMediaKit/ZLMediaKit).
 *
 * Use of this source code is governed by MIT-like license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "mk_mediakit.h"

#define LOG_LEV 4
// 修改此宏，可以选择协议类型  [AUTO-TRANSLATED:7a3e6a3a]
// Modify this macro to choose the protocol type
#define TCP_TYPE mk_type_ws

////////////////////////////////////////////////////////////////////////////////////////////////////////////
typedef struct {
    mk_tcp_session _session;
    // 下面你可以夹杂你的私货数据  [AUTO-TRANSLATED:99451203]
    // You can insert your private data below
    char your_some_useful_data[1024];
} tcp_session_user_data;
/**
 * 当tcp客户端连接服务器时触发
 * @param session 会话处理对象
 * Triggered when the tcp client connects to the server
 * @param session Session processing object
 
 * [AUTO-TRANSLATED:0030d1a6]
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
 * Receive data sent from the tcp client
 * @param session Session processing object
 * @param data Data pointer
 * @param len Data length
 
 * [AUTO-TRANSLATED:f8f01265]
 */
void API_CALL on_mk_tcp_session_data(uint16_t server_port,mk_tcp_session session, mk_buffer buffer){
    char ip[64];
    log_printf(LOG_LEV,"from %s %d, data[%d]: %s",
               mk_tcp_session_peer_ip(session,ip),
               (int)mk_tcp_session_peer_port(session),
               mk_buffer_get_size(buffer),
               mk_buffer_get_data(buffer));
    mk_tcp_session_send(session,"echo:",0);
    mk_tcp_session_send_buffer(session, buffer);
}

/**
 * 每隔2秒的定时器，用于管理超时等任务
 * @param session 会话处理对象
 * Timer every 2 seconds, used to manage timeout tasks
 * @param session Session processing object
 
 * [AUTO-TRANSLATED:f664481f]
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
 * Generally triggered by the client disconnecting tcp
 * You can call the mk_tcp_session_send_safe function in this event
 * @param session Session processing object
 * @param code Error code
 * @param msg Error message
 
 * [AUTO-TRANSLATED:62628263]
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
    // 下面你可以夹杂你的私货数据  [AUTO-TRANSLATED:99451203]
    // You can insert your private data below
    char your_some_useful_data[1024];
    int count;
} tcp_client_user_data;

/**
 * tcp客户端连接服务器成功或失败回调
 * @param client tcp客户端
 * @param code 0为连接成功，否则为失败原因
 * @param msg 连接失败错误提示
 * Callback for successful or failed connection of tcp client to server
 * @param client Tcp client
 * @param code 0 for successful connection, otherwise the reason for failure
 * @param msg Connection failure error message
 
 * [AUTO-TRANSLATED:0737893b]
 */
void API_CALL on_mk_tcp_client_connect(mk_tcp_client client,int code,const char *msg){
    log_printf(LOG_LEV,"connect result:%d %s",code,msg);
    if(code == 0){
        // 连接上后我们发送一个hello world测试数据  [AUTO-TRANSLATED:9eb05433]
        // After connecting, we send a hello world test data
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
 * Callback for disconnection between tcp client and tcp server
 * Generally caused by eof event
 * @param client Tcp client
 * @param code Error code
 * @param msg Error message
 
 * [AUTO-TRANSLATED:9cfff388]
 */
void API_CALL on_mk_tcp_client_disconnect(mk_tcp_client client,int code,const char *msg){
    log_printf(LOG_LEV,"disconnect:%d %s",code,msg);
    // 服务器主动断开了，我们销毁对象  [AUTO-TRANSLATED:4b142ba4]
    // The server actively disconnected, we destroy the object
    tcp_client_user_data *user_data = mk_tcp_client_get_user_data(client);
    mk_tcp_client_release(client);
    free(user_data);
}

/**
 * 收到tcp服务器发来的数据
 * @param client tcp客户端
 * @param data 数据指针
 * @param len 数据长度
 * Receive data sent from the tcp server
 * @param client Tcp client
 * @param data Data pointer
 * @param len Data length
 
 * [AUTO-TRANSLATED:d152eff2]
 */
void API_CALL on_mk_tcp_client_data(mk_tcp_client client, mk_buffer buffer){
    log_printf(LOG_LEV, "data[%d]:%s", mk_buffer_get_size(buffer), mk_buffer_get_data(buffer));
}

/**
 * 每隔2秒的定时器，用于管理超时等任务
 * @param client tcp客户端
 * Timer every 2 seconds, used to manage timeout tasks
 * @param client Tcp client
 
 * [AUTO-TRANSLATED:ba842db3]
 */
void API_CALL on_mk_tcp_client_manager(mk_tcp_client client){
    tcp_client_user_data *user_data = mk_tcp_client_get_user_data(client);
    char buf[1024];
    sprintf(buf,"on_mk_tcp_client_manager:%d",user_data->count);
    mk_tcp_client_send(client,buf,0);

    if(++user_data->count == 5){
        // 发送5遍后主动释放对象  [AUTO-TRANSLATED:43ce72ca]
        // Release the object after sending 5 times
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
    // 你可以连接127.0.0.1 80测试  [AUTO-TRANSLATED:68544f6e]
    // You can connect to 127.0.0.1 80 to test
//    mk_tcp_client_connect(client, "127.0.0.1", 80, 3);
}

int main(int argc, char *argv[]) {
    char *ini_path = mk_util_get_exe_dir("c_api.ini");
    char *ssl_path = mk_util_get_exe_dir("ssl.p12");

    mk_config config = {
            .ini = ini_path,
            .ini_is_path = 1,
            .log_level = 0,
            .log_mask = LOG_CONSOLE,
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

    log_info("enter any key to exit");
    getchar();

    mk_stop_all_server();
    return 0;
}