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

#ifndef SRC_PROXYPLAYER_H_
#define SRC_PROXYPLAYER_H_

#include "common.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef void* ProxyPlayerContext;

/*
 * 描述:创建一个代理播放器
 * 参数:app:应用名，生成url的一部分，rtsp://127.0.0.1/app/xxxx
 *     stream:媒体流名，生成url的一部分,rtsp://127.0.0.1/xxxx/stream
 *     rtp_type:如果播放的是rtsp连接则通过该参数配置设置rtp传输方式:RTP_TCP = 0, RTP_UDP = 1, RTP_MULTICAST = 2
 * 返回值:代理播放器句柄
 */
API_EXPORT ProxyPlayerContext API_CALL createProxyPlayer(const char *app,const char *stream,int rtp_type);

/*
 * 描述:销毁代理播放器
 * 参数:ctx:代理播放器句柄
 * 返回值:无
 */
API_EXPORT void API_CALL releaseProxyPlayer(ProxyPlayerContext ctx);


/*
 * 描述:开始播放
 * 参数:url:rtsp/rtmp连接
 * 返回值:无
 */
API_EXPORT void API_CALL proxyPlayer_play(ProxyPlayerContext ctx,const char *url);


#ifdef __cplusplus
}
#endif

#endif /* SRC_PROXYPLAYER_H_ */
