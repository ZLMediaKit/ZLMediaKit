/*
 * Copyright (c) 2016-present The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/ZLMediaKit/ZLMediaKit).
 *
 * Use of this source code is governed by MIT-like license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#include "Onvif.h"
#include "Util/util.h"
#include "Util/onceToken.h"
#include "pugixml.hpp"
#include "SoapUtil.h"
#include "Common/config.h"
#include "Common/MediaSource.h"

using namespace std;
using namespace toolkit;
using namespace mediakit;

////////////Rtp代理相关配置///////////
namespace Onvif {
#define ONVIF_FIELD "onvif."
const string kPort = ONVIF_FIELD"port";
static onceToken token([]() {
    mINI::Instance()[kPort] = 3702;
});

} //namespace RtpProxy

bool OnvifSearcher::onDeviceCB::operator()(std::map<string, string> &device_info, const std::string &onvif_url) {
    if (expired()) {
        //超时
        cb = nullptr;
        return false;
    }
    if (!cb) {
        return false;
    }
    return cb(device_info, onvif_url);
}

bool OnvifSearcher::onDeviceCB::expired() const {
    return ticker.elapsedTime() > timeout_ms;
}

/////////////////////////////////////////////////////////////////////////////////////

INSTANCE_IMP(OnvifSearcher)

OnvifSearcher::OnvifSearcher() {
    _poller = EventPollerPool::Instance().getPoller();
}

void OnvifSearcher::sendSearchBroadcast(onDevice cb, uint64_t timeout_ms) {
    weak_ptr<OnvifSearcher> weak_self = shared_from_this();
    _poller->async([weak_self, cb, timeout_ms]() mutable {
        auto strong_self = weak_self.lock();
        if (strong_self) {
            strong_self->sendSearchBroadcast_l(std::move(cb), timeout_ms);
        }
    });
}

void OnvifSearcher::sendSearchBroadcast_l(onDevice cb, uint64_t timeout_ms) {
    static struct sockaddr_in s_search_address;
    static onceToken s_token([]() {
        s_search_address.sin_family = AF_INET;
        s_search_address.sin_port = htons(3702);
        s_search_address.sin_addr.s_addr = inet_addr("239.255.255.250");
        bzero(&(s_search_address.sin_zero), sizeof s_search_address.sin_zero);
    });

    GET_CONFIG(uint16_t, onvif_port, Onvif::kPort);
    if (_sock_list.empty()) {
        for (auto &network : SockUtil::getInterfaceList()) {
            auto sock = Socket::createSocket(_poller, false);
            sock->bindUdpSock(onvif_port, network["ip"]);
            SockUtil::setBroadcast(sock->rawFD());
            weak_ptr<OnvifSearcher> weak_self = shared_from_this();
            sock->setOnRead([weak_self](const Buffer::Ptr &buf, struct sockaddr *addr, int addr_len) {
                auto strong_self = weak_self.lock();
                if (strong_self) {
                    strong_self->onDeviceResponse(buf, addr, addr_len);
                }
            });
            _sock_list.emplace_back(std::move(sock));
        }
    }

    if (!_timer) {
        weak_ptr<OnvifSearcher> weak_self = shared_from_this();
        _timer = std::make_shared<Timer>(1, [weak_self]() {
            auto strong_self = weak_self.lock();
            if (!strong_self) {
                return false;
            }
            for (auto it = strong_self->_cb_map.begin(); it != strong_self->_cb_map.end();) {
                if (it->second.expired()) {
                    it = strong_self->_cb_map.erase(it);
                    continue;
                }
                ++it;
            }
            return true;
        }, _poller);
    }

    auto uuid = SoapUtil::createUuidString();
    auto xml = SoapUtil::createDiscoveryString(uuid);
    auto &ref = _cb_map[uuid];
    ref.cb = std::move(cb);
    ref.timeout_ms = timeout_ms;

    for (auto &sock : _sock_list) {
        sock->send(xml, (struct sockaddr *) &s_search_address, sizeof(s_search_address));
    }
}

std::map<string, string> getDeviceInfo(const string &scopes) {
    std::map<string, string> keys = {{"onvif://www.onvif.org/location", "location"},
                                     {"onvif://www.onvif.org/name",     "name"},
                                     {"onvif://www.onvif.org/hardware", "hardware"}};
    std::map<string, string> ret;
    auto vec = split(scopes, " ");
    for (auto &item : vec) {
        string key;
        for (auto &pr : keys) {
            if (start_with(item, pr.first)) {
                key = pr.second;
                break;
            }
        }
        if (!key.empty()) {
            auto pos = item.rfind('/');
            ret.emplace(key, item.substr(pos + 1));
        }
    }
    return ret;
}

void OnvifSearcher::onDeviceResponse(const Buffer::Ptr &buf, struct sockaddr *addr, int addr_len) {
    try {
        SoapObject root;
        root.load(buf->data(), buf->size());
        auto uuid = root["Envelope/Header/RelatesTo"];
        auto device_service = root["Envelope/Body/ProbeMatches/ProbeMatch/XAddrs"];
        auto scopes = root["Envelope/Body/ProbeMatches/ProbeMatch/Scopes"];
        auto map = getDeviceInfo(scopes.as_xml().text().as_string());
        onGotDevice(uuid.as_xml().text().as_string(), map, device_service.as_xml().text().as_string());
    } catch (std::exception &ex) {
        WarnL << ex.what();
    }
}

static string getIpv4Url(const std::string &onvif_url) {
    auto urls = split(onvif_url, " ");
    if (urls.size() > 1) {
        for (auto url : urls) {
            MediaInfo info(url);
            if (isIP(info.host.data())) {
                return url;
            }
        }
    }
    return onvif_url;
}

void OnvifSearcher::onGotDevice(const std::string &uuid, std::map<string, string> &device_info,
                                const std::string &onvif_url) {
    auto it = _cb_map.find(uuid);
    if (it == _cb_map.end()) {
        WarnL << uuid << " " << onvif_url << " " << device_info["location"] << " " << device_info["name"] << " "
              << device_info["hardware"];
        return;
    }
    auto flag = it->second(device_info, getIpv4Url(onvif_url));
    if (!flag) {
        _cb_map.erase(it);
    }
}