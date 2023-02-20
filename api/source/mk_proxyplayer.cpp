/*
 * Copyright (c) 2016 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/xia-chu/ZLMediaKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#include "mk_proxyplayer.h"
#include "Player/PlayerProxy.h"

using namespace toolkit;
using namespace mediakit;

API_EXPORT mk_proxy_player API_CALL mk_proxy_player_create(const char *vhost, const char *app, const char *stream, int hls_enabled, int mp4_enabled) {
    assert(vhost && app && stream);
    ProtocolOption option;
    option.enable_hls = hls_enabled;
    option.enable_mp4 = mp4_enabled;
    PlayerProxy::Ptr *obj(new PlayerProxy::Ptr(new PlayerProxy(vhost, app, stream, option)));
    return (mk_proxy_player) obj;
}

API_EXPORT void API_CALL mk_proxy_player_release(mk_proxy_player ctx) {
    assert(ctx);
    PlayerProxy::Ptr *obj = (PlayerProxy::Ptr *) ctx;
    delete obj;
}

API_EXPORT void API_CALL mk_proxy_player_set_option(mk_proxy_player ctx, const char *key, const char *val){
    assert(ctx && key && val);
    PlayerProxy::Ptr &obj = *((PlayerProxy::Ptr *) ctx);
    std::string key_str(key), val_str(val);
    obj->getPoller()->async([obj,key_str,val_str](){
        //切换线程再操作
        (*obj)[key_str] = val_str;
    });
}

API_EXPORT void API_CALL mk_proxy_player_play(mk_proxy_player ctx, const char *url) {
    assert(ctx && url);
    PlayerProxy::Ptr &obj = *((PlayerProxy::Ptr *) ctx);
    std::string url_str(url);
    obj->getPoller()->async([obj,url_str](){
        //切换线程再操作
        obj->play(url_str);
    });
}

API_EXPORT void API_CALL mk_proxy_player_set_on_close(mk_proxy_player ctx, on_mk_proxy_player_close cb, void *user_data){
    mk_proxy_player_set_on_close2(ctx, cb, user_data, nullptr);
}

API_EXPORT void API_CALL mk_proxy_player_set_on_close2(mk_proxy_player ctx, on_mk_proxy_player_close cb, void *user_data, on_user_data_free user_data_free) {
    assert(ctx);
    PlayerProxy::Ptr &obj = *((PlayerProxy::Ptr *)ctx);
    std::shared_ptr<void> ptr(user_data, user_data_free ? user_data_free : [](void *) {});
    obj->getPoller()->async([obj, cb, ptr]() {
        // 切换线程再操作
        obj->setOnClose([cb, ptr](const SockException &ex) {
            if (cb) {
                cb(ptr.get(), ex.getErrCode(), ex.what(), ex.getCustomCode());
            }
        });
    });
}

API_EXPORT int API_CALL mk_proxy_player_total_reader_count(mk_proxy_player ctx){
    assert(ctx);
    PlayerProxy::Ptr &obj = *((PlayerProxy::Ptr *) ctx);
    return obj->totalReaderCount();
}
