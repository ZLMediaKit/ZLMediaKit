/*
 * MIT License
 *
 * Copyright (c) 2016 xiongziliang <771730766@qq.com>
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
#include "cleaner.h"
#include "proxyplayer.h"
#include "Device/PlayerProxy.h"
#include "Util/onceToken.h"

using namespace ZL::DEV;
using namespace ZL::Util;

static recursive_mutex s_mtxMapProxyPlayer;
static unordered_map<void *, PlayerProxy::Ptr> s_mapProxyPlayer;

static onceToken s_token([](){
    cleaner::Instance().push_front([](){
        lock_guard<recursive_mutex> lck(s_mtxMapProxyPlayer);
        s_mapProxyPlayer.clear();
        DebugL << "clear proxyplayer" << endl;
    });
},nullptr);

API_EXPORT ProxyPlayerContext API_CALL createProxyPlayer(const char *app,const char *stream,int rtp_type){
    PlayerProxy::Ptr ret(new PlayerProxy(DEFAULT_VHOST,app,stream));
    (*ret)[RtspPlayer::kRtpType] = rtp_type;

    lock_guard<recursive_mutex> lck(s_mtxMapProxyPlayer);
    s_mapProxyPlayer.emplace(ret.get(),ret);
    return ret.get();
}
API_EXPORT void API_CALL releaseProxyPlayer(ProxyPlayerContext ctx){
    lock_guard<recursive_mutex> lck(s_mtxMapProxyPlayer);
    s_mapProxyPlayer.erase(ctx);
}
API_EXPORT void API_CALL proxyPlayer_play(ProxyPlayerContext ctx,const char *url){
    PlayerProxy *ptr = (PlayerProxy *)ctx;
    ptr->play(url);
}
