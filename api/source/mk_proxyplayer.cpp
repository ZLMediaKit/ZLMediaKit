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
