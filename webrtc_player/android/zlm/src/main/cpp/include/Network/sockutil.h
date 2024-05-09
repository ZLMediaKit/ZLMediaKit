/*
 * Copyright (c) 2016 The ZLToolKit project authors. All Rights Reserved.
 *
 * This file is part of ZLToolKit(https://github.com/ZLMediaKit/ZLToolKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#ifndef NETWORK_SOCKUTIL_H
#define NETWORK_SOCKUTIL_H

#if defined(_WIN32)
#include <winsock2.h>
#include <ws2tcpip.h>
#include <iphlpapi.h>
#pragma comment (lib, "Ws2_32.lib")
#pragma comment(lib,"Iphlpapi.lib")
#else
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <net/if.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#endif // defined(_WIN32)

#include <cstring>
#include <cstdint>
#include <map>
#include <vector>
#include <string>

namespace toolkit {

#if defined(_WIN32)
#ifndef socklen_t
#define socklen_t int
#endif //!socklen_t
int ioctl(int fd, long cmd, u_long *ptr);
int close(int fd);
#endif // defined(_WIN32)

#if !defined(SOCKET_DEFAULT_BUF_SIZE)
#define SOCKET_DEFAULT_BUF_SIZE (256 * 1024)
#else
#if SOCKET_DEFAULT_BUF_SIZE == 0 && !defined(__linux__)
// just for linux, because in some high-throughput environments,
// kernel control is more efficient and reasonable than program
// settings. For example, refer to cloudflare's blog
#undef SOCKET_DEFAULT_BUF_SIZE
#define SOCKET_DEFAULT_BUF_SIZE (256 * 1024)
#endif
#endif
#define TCP_KEEPALIVE_INTERVAL 30
#define TCP_KEEPALIVE_PROBE_TIMES 9
#define TCP_KEEPALIVE_TIME 120

//套接字工具类，封装了socket、网络的一些基本操作
class SockUtil {
public:
    /**
     * 创建tcp客户端套接字并连接服务器
     * @param host 服务器ip或域名
     * @param port 服务器端口号
     * @param async 是否异步连接
     * @param local_ip 绑定的本地网卡ip
     * @param local_port 绑定的本地端口号
     * @return -1代表失败，其他为socket fd号
     */
    static int connect(const char *host, uint16_t port, bool async = true, const char *local_ip = "::", uint16_t local_port = 0);

    /**
     * 创建tcp监听套接字
     * @param port 监听的本地端口
     * @param local_ip 绑定的本地网卡ip
     * @param back_log accept列队长度
     * @return -1代表失败，其他为socket fd号
     */
    static int listen(const uint16_t port, const char *local_ip = "::", int back_log = 1024);

    /**
     * 创建udp套接字
     * @param port 监听的本地端口
     * @param local_ip 绑定的本地网卡ip
     * @param enable_reuse 是否允许重复bind端口
     * @return -1代表失败，其他为socket fd号
     */
    static int bindUdpSock(const uint16_t port, const char *local_ip = "::", bool enable_reuse = true);

    /**
     * @brief 解除与 sock 相关的绑定关系
     * @param sock, socket fd 号
     * @return 0 成功, -1 失败
     */
    static int dissolveUdpSock(int sock);

    /**
     * 开启TCP_NODELAY，降低TCP交互延时
     * @param fd socket fd号
     * @param on 是否开启
     * @return 0代表成功，-1为失败
     */
    static int setNoDelay(int fd, bool on = true);

    /**
     * 写socket不触发SIG_PIPE信号(貌似只有mac有效)
     * @param fd socket fd号
     * @return 0代表成功，-1为失败
     */
    static int setNoSigpipe(int fd);

    /**
     * 设置读写socket是否阻塞
     * @param fd socket fd号
     * @param noblock 是否阻塞
     * @return 0代表成功，-1为失败
     */
    static int setNoBlocked(int fd, bool noblock = true);

    /**
     * 设置socket接收缓存，默认貌似8K左右，一般有设置上限
     * 可以通过配置内核配置文件调整
     * @param fd socket fd号
     * @param size 接收缓存大小
     * @return 0代表成功，-1为失败
     */
    static int setRecvBuf(int fd, int size = SOCKET_DEFAULT_BUF_SIZE);

    /**
     * 设置socket接收缓存，默认貌似8K左右，一般有设置上限
     * 可以通过配置内核配置文件调整
     * @param fd socket fd号
     * @param size 接收缓存大小
     * @return 0代表成功，-1为失败
     */
    static int setSendBuf(int fd, int size = SOCKET_DEFAULT_BUF_SIZE);

    /**
     * 设置后续可绑定复用端口(处于TIME_WAITE状态)
     * @param fd socket fd号
     * @param on 是否开启该特性
     * @return 0代表成功，-1为失败
     */
    static int setReuseable(int fd, bool on = true, bool reuse_port = true);

    /**
     * 运行发送或接收udp广播信息
     * @param fd socket fd号
     * @param on 是否开启该特性
     * @return 0代表成功，-1为失败
     */
    static int setBroadcast(int fd, bool on = true);

    /**
     * 是否开启TCP KeepAlive特性
     * @param fd socket fd号
     * @param on 是否开启该特性
     * @param idle keepalive空闲时间
     * @param interval keepalive探测时间间隔
     * @param times keepalive探测次数
     * @return 0代表成功，-1为失败
     */
    static int setKeepAlive(int fd, bool on = true, int interval = TCP_KEEPALIVE_INTERVAL, int idle = TCP_KEEPALIVE_TIME, int times = TCP_KEEPALIVE_PROBE_TIMES);

    /**
     * 是否开启FD_CLOEXEC特性(多进程相关)
     * @param fd fd号，不一定是socket
     * @param on 是否开启该特性
     * @return 0代表成功，-1为失败
     */
    static int setCloExec(int fd, bool on = true);

    /**
     * 开启SO_LINGER特性
     * @param sock socket fd号
     * @param second 内核等待关闭socket超时时间，单位秒
     * @return 0代表成功，-1为失败
     */
    static int setCloseWait(int sock, int second = 0);

    /**
     * dns解析
     * @param host 域名或ip
     * @param port 端口号
     * @param addr sockaddr结构体
     * @return 是否成功
     */
    static bool getDomainIP(const char *host, uint16_t port, struct sockaddr_storage &addr, int ai_family = AF_INET,
                            int ai_socktype = SOCK_STREAM, int ai_protocol = IPPROTO_TCP, int expire_sec = 60);

    /**
     * 设置组播ttl
     * @param sock socket fd号
     * @param ttl ttl值
     * @return 0代表成功，-1为失败
     */
    static int setMultiTTL(int sock, uint8_t ttl = 64);

    /**
     * 设置组播发送网卡
     * @param sock socket fd号
     * @param local_ip 本机网卡ip
     * @return 0代表成功，-1为失败
     */
    static int setMultiIF(int sock, const char *local_ip);

    /**
     * 设置是否接收本机发出的组播包
     * @param fd socket fd号
     * @param acc 是否接收
     * @return 0代表成功，-1为失败
     */
    static int setMultiLOOP(int fd, bool acc = false);

    /**
     * 加入组播
     * @param fd socket fd号
     * @param addr 组播地址
     * @param local_ip 本机网卡ip
     * @return 0代表成功，-1为失败
     */
    static int joinMultiAddr(int fd, const char *addr, const char *local_ip = "0.0.0.0");

    /**
     * 退出组播
     * @param fd socket fd号
     * @param addr 组播地址
     * @param local_ip 本机网卡ip
     * @return 0代表成功，-1为失败
     */
    static int leaveMultiAddr(int fd, const char *addr, const char *local_ip = "0.0.0.0");

    /**
     * 加入组播并只接受该源端的组播数据
     * @param sock socket fd号
     * @param addr 组播地址
     * @param src_ip 数据源端地址
     * @param local_ip  本机网卡ip
     * @return 0代表成功，-1为失败
     */
    static int joinMultiAddrFilter(int sock, const char *addr, const char *src_ip, const char *local_ip = "0.0.0.0");

    /**
     * 退出组播
     * @param fd socket fd号
     * @param addr 组播地址
     * @param src_ip 数据源端地址
     * @param local_ip  本机网卡ip
     * @return 0代表成功，-1为失败
     */
    static int leaveMultiAddrFilter(int fd, const char *addr, const char *src_ip, const char *local_ip = "0.0.0.0");

    /**
     * 获取该socket当前发生的错误
     * @param fd socket fd号
     * @return 错误代码
     */
    static int getSockError(int fd);

    /**
     * 获取网卡列表
     * @return vector<map<ip:name> >
     */
    static std::vector<std::map<std::string, std::string>> getInterfaceList();

    /**
     * 获取本机默认网卡ip
     */
    static std::string get_local_ip();

    /**
     * 获取该socket绑定的本地ip
     * @param sock socket fd号
     */
    static std::string get_local_ip(int sock);

    /**
     * 获取该socket绑定的本地端口
     * @param sock socket fd号
     */
    static uint16_t get_local_port(int sock);

    /**
     * 获取该socket绑定的远端ip
     * @param sock socket fd号
     */
    static std::string get_peer_ip(int sock);

    /**
     * 获取该socket绑定的远端端口
     * @param sock socket fd号
     */
    static uint16_t get_peer_port(int sock);

    static bool support_ipv6();
    /**
     * 线程安全的in_addr转ip字符串
     */
    static std::string inet_ntoa(const struct in_addr &addr);
    static std::string inet_ntoa(const struct in6_addr &addr);
    static std::string inet_ntoa(const struct sockaddr *addr);
    static uint16_t inet_port(const struct sockaddr *addr);
    static struct sockaddr_storage make_sockaddr(const char *ip, uint16_t port);
    static socklen_t get_sock_len(const struct sockaddr *addr);
    static bool get_sock_local_addr(int fd, struct sockaddr_storage &addr);
    static bool get_sock_peer_addr(int fd, struct sockaddr_storage &addr);

    /**
     * 获取网卡ip
     * @param if_name 网卡名
     */
    static std::string get_ifr_ip(const char *if_name);

    /**
     * 获取网卡名
     * @param local_op 网卡ip
     */
    static std::string get_ifr_name(const char *local_op);

    /**
     * 根据网卡名获取子网掩码
     * @param if_name 网卡名
     */
    static std::string get_ifr_mask(const char *if_name);

    /**
     * 根据网卡名获取广播地址
     * @param if_name 网卡名
     */
    static std::string get_ifr_brdaddr(const char *if_name);

    /**
     * 判断两个ip是否为同一网段
     * @param src_ip 我的ip
     * @param dts_ip 对方ip
     */
    static bool in_same_lan(const char *src_ip, const char *dts_ip);

    /**
     * 判断是否为ipv4地址
     */
    static bool is_ipv4(const char *str);

    /**
     * 判断是否为ipv6地址
     */
    static bool is_ipv6(const char *str);
};

}  // namespace toolkit
#endif // !NETWORK_SOCKUTIL_H
