/*
 * MIT License
 *
 * Copyright (c) 2019 xiongziliang <771730766@qq.com>
 *
 * This file is part of ZLMediaKit(https://github.com/xiongziliang/ZLMediaKit).
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
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
