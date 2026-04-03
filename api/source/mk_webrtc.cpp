/*
 * Copyright (c) 2016-present The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/ZLMediaKit/ZLMediaKit).
 *
 * Use of this source code is governed by MIT-like license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#include "mk_webrtc.h"
#include "mk_util.h"

#include <stdarg.h>
#include <unordered_map>
#include "Util/logger.h"
#include "Util/SSLBox.h"
#include "Util/File.h"
#include "Network/TcpServer.h"
#include "Network/UdpServer.h"
#include "Thread/WorkThreadPool.h"

#include "Rtsp/RtspSession.h"
#include "Rtmp/RtmpSession.h"
#include "Http/HttpSession.h"
#include "Shell/ShellSession.h"
#include "Player/PlayerProxy.h"

using namespace std;
using namespace toolkit;
using namespace mediakit;

#ifdef ENABLE_WEBRTC

#include "webrtc/WebRtcProxyPlayer.h"
#include "webrtc/WebRtcProxyPlayerImp.h"
#include "webrtc/WebRtcSignalingPeer.h"
#include "webrtc/WebRtcSignalingSession.h"
#include "webrtc/WebRtcSession.h"

static UdpServer::Ptr rtcServer_udp;
static TcpServer::Ptr rtcServer_tcp;
class WebRtcArgsUrl : public mediakit::WebRtcArgs {
public:
    WebRtcArgsUrl(std::string url) { _url = std::move(url); }

    toolkit::variant operator[](const std::string &key) const override {
        if (key == "url") {
            return _url;
        }
        return "";
    }

private:
    std::string _url;
};
#endif

API_EXPORT void API_CALL mk_webrtc_get_answer_sdp(void *user_data, on_mk_webrtc_get_answer_sdp cb, const char *type, const char *offer, const char *url) {
    mk_webrtc_get_answer_sdp2(user_data, nullptr, cb, type, offer, url);
}
API_EXPORT void API_CALL mk_webrtc_get_answer_sdp2(
    void *user_data, on_user_data_free user_data_free, on_mk_webrtc_get_answer_sdp cb, const char *type, const char *offer, const char *url) {
#ifdef ENABLE_WEBRTC
    assert(type && offer && url && cb);
    auto session = std::make_shared<HttpSession>(Socket::createSocket());
    std::string offer_str = offer;
    std::shared_ptr<void> ptr(user_data, user_data_free ? user_data_free : [](void *) {});
    auto args = std::make_shared<WebRtcArgsUrl>(url);
    WebRtcPluginManager::Instance().negotiateSdp(*session, type, *args, [offer_str, session, ptr, cb](const WebRtcInterface &exchanger) mutable {
        auto &handler = const_cast<WebRtcInterface &>(exchanger);
        try {
            auto sdp_answer = handler.getAnswerSdp(offer_str);
            cb(ptr.get(), sdp_answer.data(), nullptr);
        } catch (std::exception &ex) {
            cb(ptr.get(), nullptr, ex.what());
        }
    });
#else
    WarnL << "未启用webrtc功能, 编译时请开启ENABLE_WEBRTC";
#endif
}

API_EXPORT void API_CALL mk_webrtc_get_proxy_player_info(mk_proxy_player ctx, on_mk_webrtc_get_proxy_player_info_cb cb) {
#ifdef ENABLE_WEBRTC
    assert(ctx && cb);
    PlayerProxy::Ptr *obj = (PlayerProxy::Ptr *)ctx;
    auto media_player = obj->get()->getDelegate();
    if (!media_player) {
        cb(nullptr, "Media player not found");
        return;
    }

    auto webrtc_player_imp = std::dynamic_pointer_cast<WebRtcProxyPlayerImp>(media_player);
    if (!webrtc_player_imp) {
        cb(nullptr, "Stream proxy is not WebRTC type");
        return;
    }

    auto webrtc_transport = webrtc_player_imp->getWebRtcTransport();
    if (!webrtc_transport) {
        cb(nullptr, "WebRTC transport not available");
        return;
    }

    webrtc_transport->getTransportInfo([cb](Json::Value transport_info) mutable {
        if (transport_info.isMember("error")) {
            cb(nullptr, strdup(transport_info["error"].asCString()));
            return;
        }
        cb(strdup(transport_info.toStyledString().c_str()), "");
    });
#else
    WarnL << "未启用webrtc功能, 编译时请开启ENABLE_WEBRTC";
#endif
}

API_EXPORT void API_CALL mk_webrtc_add_room_keeper(
    const char *room_id, const char *server_host, uint16_t server_port, int ssl, on_mk_webrtc_room_keeper_info_cb cb, void *user_data) {
    mk_webrtc_add_room_keeper2(room_id, server_host, server_port, ssl, cb, user_data, nullptr);
}

API_EXPORT void API_CALL mk_webrtc_add_room_keeper2(
    const char *room_id, const char *server_host, uint16_t server_port, int ssl, on_mk_webrtc_room_keeper_info_cb cb, void *user_data,
    on_user_data_free user_data_free) {
#ifdef ENABLE_WEBRTC
    assert(server_host && server_port && room_id && cb);
    // server_host: 信令服务器host
    // server_post: 信令服务器host
    // room_id: 注册的id,信令服务器会对该id进行唯一性检查
    std::string server_host_str(server_host), room_id_str(room_id);
    std::shared_ptr<void> ptr(user_data, user_data_free ? user_data_free : [](void *) {});
    addWebrtcRoomKeeper(server_host_str, server_port, room_id_str, ssl, [ptr,cb](const SockException &ex, const string &key) mutable {
        if (ex) {
            cb(ptr.get(), nullptr, ex.what());
        } else {
            cb(ptr.get(), key.c_str(), nullptr);
        }
    });
#else
    WarnL << "未启用webrtc功能, 编译时请开启ENABLE_WEBRTC";
#endif
}

API_EXPORT void API_CALL mk_webrtc_del_room_keeper(const char *room_key, on_mk_webrtc_room_keeper_info_cb cb, void *user_data) {
    mk_webrtc_del_room_keeper2(room_key,cb,user_data,nullptr);
}

API_EXPORT void API_CALL
mk_webrtc_del_room_keeper2(const char *room_key, on_mk_webrtc_room_keeper_info_cb cb, void *user_data, on_user_data_free user_data_free) {
#ifdef ENABLE_WEBRTC
    assert(room_key && cb);
    std::string room_key_str(room_key);
    std::shared_ptr<void> ptr(user_data, user_data_free ? user_data_free : [](void *) {});
    delWebrtcRoomKeeper(room_key_str, [room_key_str, ptr, cb](const SockException &ex) mutable {
        if (ex) {
            cb(ptr.get(), room_key_str.c_str(), ex.what());
        }
        cb(ptr.get(), room_key_str.c_str(), nullptr);
    });
#else
    WarnL << "未启用webrtc功能, 编译时请开启ENABLE_WEBRTC";
#endif
}

API_EXPORT void API_CALL mk_webrtc_list_room_keeper(on_mk_webrtc_room_keeper_data_cb cb) {
#ifdef ENABLE_WEBRTC
    assert(cb);
    listWebrtcRoomKeepers([cb](const std::string &key, const WebRtcSignalingPeer::Ptr &p) {
        Json::Value item = ToJson(p);
        item["room_key"] = key;
        cb(strdup(item.toStyledString().c_str()));
    });
#else
    WarnL << "未启用webrtc功能, 编译时请开启ENABLE_WEBRTC";
#endif
}

API_EXPORT void API_CALL mk_webrtc_list_rooms(on_mk_webrtc_room_keeper_data_cb cb){
#ifdef ENABLE_WEBRTC
    assert(cb);
    listWebrtcRooms([cb](const std::string &key, const WebRtcSignalingSession::Ptr &p) {
        Json::Value item = ToJson(p);
        item["room_id"] = key;
        cb(strdup(item.toStyledString().c_str()));
    });
#else
    WarnL << "未启用webrtc功能, 编译时请开启ENABLE_WEBRTC";
#endif
}
