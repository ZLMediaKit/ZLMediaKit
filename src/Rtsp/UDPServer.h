/*
 * Copyright (c) 2016 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/xia-chu/ZLMediaKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#ifndef RTSP_UDPSERVER_H_
#define RTSP_UDPSERVER_H_

#include <stdint.h>
#include <mutex>
#include <memory>
#include <unordered_map>
#include <unordered_set>
#include "Util/util.h"
#include "Network/Socket.h"

namespace mediakit {

class UDPServer : public std::enable_shared_from_this<UDPServer> {
    UDPServer();
public:
    ~UDPServer();
    static UDPServer &Instance();
    // 监听udp本地地址和端口，并接收数据
    toolkit::Socket::Ptr getSock(toolkit::SocketHelper &helper, const char *local_ip, int interleaved, uint16_t local_port = 0);

    // 数据回调，当函数返回false时，则自动取消回调的注册
    using onRecvData = std::function<bool(int intervaled, const toolkit::Buffer::Ptr &buffer, struct sockaddr *peer_addr)>;
    // 注册某个目的地址的包数据回调
    void listenPeer(const char *peer_ip, void *obj, const onRecvData &cb);
    // 取消注册回调
    void stopListenPeer(const char *peer_ip, void *obj);

private:
    std::mutex _mtx_udp_sock;
    std::mutex _mtx_on_recv;
    // local_ip:interleaved -> socket
    std::unordered_map<std::string, toolkit::Socket::Ptr> _udp_sock_map;
    // peer_ip -> obj -> onRecvData
    std::unordered_map<std::string, std::unordered_map<void *, onRecvData> > _on_recv_map;
};

} /* namespace mediakit */

#endif /* RTSP_UDPSERVER_H_ */
