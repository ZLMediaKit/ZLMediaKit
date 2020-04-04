/*
 * Copyright (c) 2016 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/xiongziliang/ZLMediaKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#include <assert.h>
#include "mk_pusher.h"
#include "Pusher/MediaPusher.h"
using namespace mediakit;

API_EXPORT mk_pusher API_CALL mk_pusher_create(const char *schema,const char *vhost,const char *app, const char *stream){
    assert(schema && vhost && app && schema);
    MediaPusher::Ptr *obj = new MediaPusher::Ptr(new MediaPusher(schema,vhost,app,stream));
    return obj;
}

API_EXPORT void API_CALL mk_pusher_release(mk_pusher ctx){
    assert(ctx);
    MediaPusher::Ptr *obj = (MediaPusher::Ptr *)ctx;
    delete obj;
}

API_EXPORT void API_CALL mk_pusher_set_option(mk_pusher ctx, const char *key, const char *val){
    assert(ctx && key && val);
    MediaPusher::Ptr &obj = *((MediaPusher::Ptr *)ctx);
    string key_str(key),val_str(val);
    obj->getPoller()->async([obj,key_str,val_str](){
        //切换线程再操作
        (*obj)[key_str] = val_str;
    });
}

API_EXPORT void API_CALL mk_pusher_publish(mk_pusher ctx,const char *url){
    assert(ctx && url);
    MediaPusher::Ptr &obj = *((MediaPusher::Ptr *)ctx);
    string url_str(url);
    obj->getPoller()->async([obj,url_str](){
        //切换线程再操作
        obj->publish(url_str);
    });
}

API_EXPORT void API_CALL mk_pusher_set_on_result(mk_pusher ctx, on_mk_push_event cb, void *user_data){
    assert(ctx && cb);
    MediaPusher::Ptr &obj = *((MediaPusher::Ptr *)ctx);
    obj->getPoller()->async([obj,cb,user_data](){
        //切换线程再操作
        obj->setOnPublished([cb,user_data](const SockException &ex){
            cb(user_data,ex.getErrCode(),ex.what());
        });
    });
}

API_EXPORT void API_CALL mk_pusher_set_on_shutdown(mk_pusher ctx, on_mk_push_event cb, void *user_data){
    assert(ctx && cb);
    MediaPusher::Ptr &obj = *((MediaPusher::Ptr *)ctx);
    obj->getPoller()->async([obj,cb,user_data](){
        //切换线程再操作
        obj->setOnShutdown([cb,user_data](const SockException &ex){
            cb(user_data,ex.getErrCode(),ex.what());
        });
    });
}
