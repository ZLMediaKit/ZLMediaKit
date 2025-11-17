/*
 * Copyright (c) 2016-present The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/ZLMediaKit/ZLMediaKit).
 *
 * Use of this source code is governed by MIT-like license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#ifndef ZLMEDIAKIT_ONVIF_H
#define ZLMEDIAKIT_ONVIF_H

#include <vector>
#include <map>
#include "Network/Socket.h"
#include "Network/Buffer.h"

class OnvifSearcher : public std::enable_shared_from_this<OnvifSearcher> {
public:
    //返回false代表不再监听事件
    using onDevice = std::function<bool(const std::map<std::string, std::string> &device_info, const std::string &onvif_url)>;
    OnvifSearcher();

    static OnvifSearcher &Instance();
    void sendSearchBroadcast(onDevice cb = nullptr, uint64_t timeout_ms = 10 * 1000);

private:
    void onDeviceResponse(const toolkit::Buffer::Ptr &buf, struct sockaddr *addr, int addr_len);
    void onGotDevice(const std::string &uuid, std::map<std::string, std::string> &device_info, const std::string &onvif_url);
    void sendSearchBroadcast_l(onDevice cb, uint64_t timeout_ms);

private:
    struct onDeviceCB{
        onDevice cb;
        toolkit::Ticker ticker;
        uint64_t timeout_ms;

        bool expired() const;
        bool operator()(std::map<std::string, std::string> &device_info, const std::string &onvif_url);
    };

private:
    toolkit::EventPoller::Ptr _poller;
    toolkit::Timer::Ptr _timer;
    std::vector<toolkit::Socket::Ptr> _sock_list;
    std::unordered_map<std::string/*uuid*/, onDeviceCB> _cb_map;
};

#endif //ZLMEDIAKIT_ONVIF_H
