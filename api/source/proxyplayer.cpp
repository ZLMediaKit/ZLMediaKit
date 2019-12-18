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

#include "proxyplayer.h"
#include "Player/PlayerProxy.h"

using namespace toolkit;
using namespace mediakit;

API_EXPORT mk_proxy_player API_CALL mk_proxy_player_create(const char *app, const char *stream, int rtp_type, int hls_enabled, int mp4_enabled) {
    PlayerProxy::Ptr *obj(new PlayerProxy::Ptr(new PlayerProxy(DEFAULT_VHOST, app, stream, true, true, hls_enabled, mp4_enabled)));
    (**obj)[Client::kRtpType] = rtp_type;
    return (mk_proxy_player) obj;
}

API_EXPORT void API_CALL mk_proxy_player_release(mk_proxy_player ctx) {
    PlayerProxy::Ptr *obj = (PlayerProxy::Ptr *) ctx;
    delete obj;
}

API_EXPORT void API_CALL mk_proxy_player_play(mk_proxy_player ctx, const char *url) {
    PlayerProxy::Ptr *obj = (PlayerProxy::Ptr *) ctx;
    (*obj)->play(url);
}
