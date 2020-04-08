/*
 * Copyright (c) 2016 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/xiongziliang/ZLMediaKit).
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
    PlayerProxy::Ptr *obj(new PlayerProxy::Ptr(new PlayerProxy(vhost, app, stream, true, true, hls_enabled, mp4_enabled)));
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
    string key_str(key),val_str(val);
    obj->getPoller()->async([obj,key_str,val_str](){
        //切换线程再操作
        (*obj)[key_str] = val_str;
    });
}

API_EXPORT void API_CALL mk_proxy_player_play(mk_proxy_player ctx, const char *url) {
    assert(ctx && url);
    PlayerProxy::Ptr &obj = *((PlayerProxy::Ptr *) ctx);
    string url_str(url);
    obj->getPoller()->async([obj,url_str](){
        //切换线程再操作
        obj->play(url_str);
    });
}

API_EXPORT void API_CALL mk_proxy_player_set_on_close(mk_proxy_player ctx, on_mk_proxy_player_close cb, void *user_data){
    assert(ctx);
    PlayerProxy::Ptr &obj = *((PlayerProxy::Ptr *) ctx);
    obj->getPoller()->async([obj,cb,user_data](){
        //切换线程再操作
        obj->setOnClose([cb,user_data](){
            if(cb){
                cb(user_data);
            }
        });
    });
}

API_EXPORT int API_CALL mk_proxy_player_total_reader_count(mk_proxy_player ctx){
    assert(ctx);
    PlayerProxy::Ptr &obj = *((PlayerProxy::Ptr *) ctx);
    return obj->totalReaderCount();
}
